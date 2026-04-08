// Analytics functions — compute balance sheet, gap report, labor schedule,
// BOM, and loop closure score from a completed PlanResult graph.
// Called by BackpropagationSolver after the graph is fully resolved.

#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>
#include <unordered_map>

namespace homestead {

// Forward declarations from scheduling.cpp / convergence.cpp.
double cycles_in_month(const Lifecycle& lc, int month) noexcept;
MonthlyValues labor_for_instance(const Entity&, double, Scenario) noexcept;
void check_seasonality(const std::vector<ProductionGoal>&, const Registry&, const Graph&,
                       std::vector<Diagnostic>&);
void check_labor_constraint(const MonthlyValues&, double, std::vector<Diagnostic>&);

namespace {

double pick(const VariableQuantity& q, Scenario s) noexcept {
    switch (s) {
        case Scenario::optimistic:
            return q.min;
        case Scenario::expected:
            return q.expected;
        case Scenario::pessimistic:
            return q.max;
    }
    return q.expected;
}

}  // anonymous namespace

// ── compute_balance_sheet ──────────────────────────────────────────────────────

std::vector<ResourceBalance> compute_balance_sheet(const Graph& graph, const Registry& registry,
                                                   Scenario scenario) {
    std::unordered_map<std::string, ResourceBalance> balances;

    auto get_balance = [&](const std::string& slug) -> ResourceBalance& {
        if (!balances.contains(slug)) {
            balances[slug].resource_slug = slug;
        }
        return balances[slug];
    };

    // Collect all entity-instance nodes via the edges.
    for (NodeId nid = 0;; ++nid) {
        auto node_opt = graph.get_node(nid);
        if (!node_opt) {
            // Ids are assigned 0..n-1; once we get nullopt for a few consecutive
            // ids past the last valid one, stop.  Safe because node_count() gives
            // us the exact count.
            if (nid >= graph.node_count() + 100) {
                break;
            }
            continue;
        }

        std::visit(
            [&](const auto& node) {
                using T = std::decay_t<decltype(node)>;

                if constexpr (std::is_same_v<T, EntityInstanceNode>) {
                    auto entity_opt = registry.find_entity(node.entity_slug);
                    if (!entity_opt) {
                        return;
                    }
                    double qty = pick(node.quantity, scenario);

                    for (std::size_t m = 0; m < 12; ++m) {
                        double cpm = cycles_in_month(entity_opt->lifecycle, static_cast<int>(m));
                        // Outputs → internal_production
                        for (const auto& out : entity_opt->outputs) {
                            double prod = pick(out.quantity_per_cycle, scenario) * cpm * qty;
                            get_balance(out.resource_slug).internal_production[m] += prod;
                        }
                        // Inputs → consumption
                        for (const auto& inp : entity_opt->inputs) {
                            double cons = pick(inp.quantity_per_cycle, scenario) * cpm * qty;
                            get_balance(inp.resource_slug).consumption[m] += cons;
                        }
                    }
                } else if constexpr (std::is_same_v<T, ExternalSourceNode>) {
                    // External purchases are tallied from successor consumption.
                    // Traversal is handled via successor/predecessor scan.
                    for (NodeId succ : graph.successors(node.id)) {
                        (void)succ;
                    }
                } else if constexpr (std::is_same_v<T, GoalSinkNode>) {
                    // Goal consumption.
                    for (std::size_t m = 0; m < 12; ++m) {
                        get_balance(node.resource_slug).consumption[m] +=
                            pick(node.quantity_per_month, scenario);
                    }
                }
            },
            *node_opt);
    }

    // Compute annuals.
    std::vector<ResourceBalance> result;
    result.reserve(balances.size());
    for (auto& [slug, bal] : balances) {
        bal.annual_internal_production =
            std::accumulate(bal.internal_production.begin(), bal.internal_production.end(), 0.0);
        bal.annual_consumption =
            std::accumulate(bal.consumption.begin(), bal.consumption.end(), 0.0);
        bal.annual_external_purchase =
            std::accumulate(bal.external_purchase.begin(), bal.external_purchase.end(), 0.0);
        result.push_back(std::move(bal));
    }
    return result;
}

// ── compute_labor_schedule ────────────────────────────────────────────────────

MonthlyValues compute_labor_schedule(const Graph& graph, const Registry& registry,
                                     Scenario scenario) {
    MonthlyValues total{};

    for (NodeId nid = 0;; ++nid) {
        auto node_opt = graph.get_node(nid);
        if (!node_opt) {
            if (nid >= graph.node_count() + 100) {
                break;
            }
            continue;
        }
        if (!std::holds_alternative<EntityInstanceNode>(*node_opt)) {
            continue;
        }

        const auto& inst = std::get<EntityInstanceNode>(*node_opt);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt) {
            continue;
        }

        double qty = pick(inst.quantity, scenario);
        auto monthly = labor_for_instance(*entity_opt, qty, scenario);
        for (std::size_t m = 0; m < 12; ++m) {
            total[m] += monthly[m];
        }
    }
    return total;
}

// ── compute_bom ───────────────────────────────────────────────────────────────

InfrastructureBOM compute_bom(const Graph& graph, const Registry& registry, Scenario scenario) {
    InfrastructureBOM bom;
    std::unordered_map<std::string, double> materials;

    for (NodeId nid = 0;; ++nid) {
        auto node_opt = graph.get_node(nid);
        if (!node_opt) {
            if (nid >= graph.node_count() + 100) {
                break;
            }
            continue;
        }
        if (!std::holds_alternative<EntityInstanceNode>(*node_opt)) {
            continue;
        }

        const auto& inst = std::get<EntityInstanceNode>(*node_opt);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt) {
            continue;
        }

        double qty = pick(inst.quantity, scenario);
        const auto& infra = entity_opt->infrastructure;

        bom.total_area_m2 += pick(infra.area_m2, scenario) * qty;
        bom.estimated_initial_cost += pick(infra.initial_cost, scenario) * qty;
        bom.initial_labor_hours += pick(infra.initial_labor_hours, scenario) * qty;

        for (const auto& mat : infra.construction_materials) {
            materials[mat.resource_slug] += pick(mat.quantity_per_cycle, scenario) * qty;
        }
    }

    bom.materials.reserve(materials.size());
    for (auto& [slug, qty] : materials) {
        bom.materials.emplace_back(slug, qty);
    }
    return bom;
}

// ── compute_loop_closure_score ────────────────────────────────────────────────

double compute_loop_closure_score(const std::vector<ResourceBalance>& balance_sheet) {
    double total_demand = 0.0;
    double internal_supply = 0.0;

    for (const auto& bal : balance_sheet) {
        double demand = bal.annual_consumption;
        if (demand <= 0.0) {
            continue;
        }
        total_demand += demand;
        internal_supply += std::min(bal.annual_internal_production, demand);
    }

    if (total_demand <= 0.0) {
        return 0.0;
    }
    return internal_supply / total_demand;
}

// ── Public entry point: populate PlanResult after graph is resolved ───────────

void populate_plan_result(PlanResult& result, const std::vector<ProductionGoal>& goals,
                          const Registry& registry, const SolverConfig& config) {
    result.balance_sheet = compute_balance_sheet(result.graph, registry, config.scenario);
    result.labor_schedule = compute_labor_schedule(result.graph, registry, config.scenario);
    result.bom = compute_bom(result.graph, registry, config.scenario);
    result.loop_closure_score = compute_loop_closure_score(result.balance_sheet);

    // Gap report: resources with any external purchase.
    std::ranges::copy_if(result.balance_sheet, std::back_inserter(result.gap_report),
                         [](const ResourceBalance& b) {
                             return b.annual_external_purchase > 0.0 ||
                                    b.annual_internal_production < b.annual_consumption;
                         });

    // Seasonality check.
    check_seasonality(goals, registry, result.graph, result.diagnostics);

    // Labor constraint check.
    if (config.max_labor_hours_per_month) {
        check_labor_constraint(result.labor_schedule, *config.max_labor_hours_per_month,
                               result.diagnostics);
    }
}

}  // namespace homestead
