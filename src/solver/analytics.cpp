// Analytics functions — compute balance sheet, gap report, labor schedule,
// BOM, and loop closure score from a completed PlanResult graph.
// Called by BackpropagationSolver after the graph is fully resolved.

#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>
#include <unordered_map>

#include "detail/pick.hpp"

namespace homestead {

// Forward declarations from scheduling.cpp / convergence.cpp.
double cycles_in_month(const Lifecycle& lc, int month) noexcept;
MonthlyValues labor_for_instance(const Entity&, double, Scenario) noexcept;
void check_seasonality(const std::vector<ProductionGoal>&, const Registry&, const Graph&,
                       std::vector<Diagnostic>&);
void check_labor_constraint(const MonthlyValues&, double, std::vector<Diagnostic>&);
void check_nutrient_deficits(const std::optional<NutrientBalance>&, std::vector<Diagnostic>&);

// ── Nutrient balance helpers ──────────────────────────────────────────────────

namespace {

struct NpkGrams {
    double n{};
    double p{};
    double k{};
};

// Returns grams of N, P, K from a resource's composition map and a net quantity.
// unit_is_kg: true for solid resources (N_percent keys), false for liquids (N_ppm keys).
NpkGrams extract_npk(const std::unordered_map<std::string, double>& composition,
                     double net_quantity, bool unit_is_kg) noexcept {
    NpkGrams g;
    if (unit_is_kg) {
        // N_percent = % of dry weight → grams = kg × percent/100 × 1000
        if (auto it = composition.find("N_percent"); it != composition.end()) {
            g.n = net_quantity * it->second / 100.0 * 1000.0;
        }
        if (auto it = composition.find("P_percent"); it != composition.end()) {
            g.p = net_quantity * it->second / 100.0 * 1000.0;
        }
        if (auto it = composition.find("K_percent"); it != composition.end()) {
            g.k = net_quantity * it->second / 100.0 * 1000.0;
        }
    } else {
        // N_ppm = mg/L → grams = liters × ppm / 1000
        if (auto it = composition.find("N_ppm"); it != composition.end()) {
            g.n = net_quantity * it->second / 1000.0;
        }
        if (auto it = composition.find("P_ppm"); it != composition.end()) {
            g.p = net_quantity * it->second / 1000.0;
        }
        // K_ppm absent (e.g., biofertilizer_l) → g.k remains 0.0 (Q3 default)
    }
    return g;
}

}  // anonymous namespace

// ── compute_nutrient_balance ──────────────────────────────────────────────────

// Computes per-month N/P/K supply (net fertilizer output) and demand (crop
// entities). Returns nullopt when the solved graph contains no area-based crop
// entity instances (nothing to balance). This is a read-only post-solve pass
// and does not alter any solver state (FR-008).
std::optional<NutrientBalance> compute_nutrient_balance(
    const Graph& graph, const Registry& registry, const std::vector<ResourceBalance>& balance_sheet,
    Scenario scenario) {
    // ── Supply side ──────────────────────────────────────────────────────────
    // Available nutrient = gross internal production of a fertilizer resource
    // minus the quantity of that resource consumed by non-crop (intermediate)
    // entities within the same plan. This prevents double-counting when e.g.
    // compost flows into an earthworm bin before reaching crop beds (Q4).

    // Build: resource_slug → gross production per month (from already-computed
    // balance_sheet).
    std::unordered_map<std::string, MonthlyValues> gross_prod;
    for (const auto& bal : balance_sheet) {
        gross_prod[bal.resource_slug] = bal.internal_production;
    }

    // Build: resource_slug → quantity consumed by non-crop entity instances per month.
    std::unordered_map<std::string, MonthlyValues> feedstock_consumed;
    graph.for_each_node([&](NodeId /*id*/, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv)) {
            return;
        }
        const auto& inst = std::get<EntityInstanceNode>(nv);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt || entity_opt->nutrient_demand.has_value()) {
            return;  // skip crops
        }
        double qty = detail::pick(inst.quantity, scenario);
        for (std::size_t m = 0; m < 12; ++m) {
            double cpm = cycles_in_month(entity_opt->lifecycle, static_cast<int>(m));
            for (const auto& inp : entity_opt->inputs) {
                feedstock_consumed[inp.resource_slug][m] +=
                    detail::pick(inp.quantity_per_cycle, scenario) * cpm * qty;
            }
        }
    });

    NutrientBalance balance;
    bool has_fertilizer = false;

    for (const auto& [slug, prod_monthly] : gross_prod) {
        auto res_opt = registry.find_resource(slug);
        if (!res_opt || res_opt->composition.empty()) {
            continue;
        }
        const auto& comp = res_opt->composition;
        bool has_npk = comp.contains("N_percent") || comp.contains("N_ppm") ||
                       comp.contains("P_percent") || comp.contains("P_ppm") ||
                       comp.contains("K_percent");
        if (!has_npk) {
            continue;
        }

        bool is_liquid = comp.contains("N_ppm") || comp.contains("P_ppm");
        const MonthlyValues& feedstock =
            feedstock_consumed.contains(slug) ? feedstock_consumed.at(slug) : MonthlyValues{};

        for (std::size_t m = 0; m < 12; ++m) {
            double net = std::max(0.0, prod_monthly[m] - feedstock[m]);
            if (net <= 0.0) {
                continue;
            }
            has_fertilizer = true;
            auto npk = extract_npk(comp, net, !is_liquid);
            balance.available_n[m] += npk.n;
            balance.available_p[m] += npk.p;
            balance.available_k[m] += npk.k;
        }
    }

    // ── Demand side ──────────────────────────────────────────────────────────
    bool has_demand = false;
    graph.for_each_node([&](NodeId /*id*/, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv)) {
            return;
        }
        const auto& inst = std::get<EntityInstanceNode>(nv);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt || !entity_opt->nutrient_demand) {
            return;
        }

        const auto& nd = *entity_opt->nutrient_demand;
        double qty = detail::pick(inst.quantity, scenario);
        double area = detail::pick(entity_opt->infrastructure.area_m2, scenario);

        for (std::size_t m = 0; m < 12; ++m) {
            // cycles_in_month returns cycles_per_year / active_month_count for active
            // months — proportional distribution satisfying the prorated Q1 requirement.
            double frac = cycles_in_month(entity_opt->lifecycle, static_cast<int>(m));
            double scale = area * qty * frac;
            if (scale <= 0.0) {
                continue;
            }
            balance.demanded_n[m] += nd.n_g_per_m2_per_cycle * scale;
            balance.demanded_p[m] += nd.p_g_per_m2_per_cycle * scale;
            balance.demanded_k[m] += nd.k_g_per_m2_per_cycle * scale;
            has_demand = true;
        }
    });

    // nullopt iff no crop entity instances were present (nothing to balance).
    // FR-008: fertilizer supply alone is not sufficient to activate the balance.
    if (!has_demand) {
        return std::nullopt;
    }
    return balance;
}

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

    graph.for_each_node([&](NodeId /*id*/, const NodeVariant& node_var) {
        std::visit(
            [&](const auto& node) {
                using T = std::decay_t<decltype(node)>;

                if constexpr (std::is_same_v<T, EntityInstanceNode>) {
                    auto entity_opt = registry.find_entity(node.entity_slug);
                    if (!entity_opt) {
                        return;
                    }
                    double qty = detail::pick(node.quantity, scenario);

                    for (std::size_t m = 0; m < 12; ++m) {
                        double cpm = cycles_in_month(entity_opt->lifecycle, static_cast<int>(m));
                        // Outputs → internal_production
                        for (const auto& out : entity_opt->outputs) {
                            double prod =
                                detail::pick(out.quantity_per_cycle, scenario) * cpm * qty;
                            get_balance(out.resource_slug).internal_production[m] += prod;
                        }
                        // Inputs → consumption
                        for (const auto& inp : entity_opt->inputs) {
                            double cons =
                                detail::pick(inp.quantity_per_cycle, scenario) * cpm * qty;
                            get_balance(inp.resource_slug).consumption[m] += cons;
                        }
                    }
                } else if constexpr (std::is_same_v<T, ExternalSourceNode>) {
                    // External purchases are tallied from successor consumption.
                    for (NodeId succ : graph.successors(node.id)) {
                        (void)succ;
                    }
                } else if constexpr (std::is_same_v<T, GoalSinkNode>) {
                    // Goal consumption.
                    for (std::size_t m = 0; m < 12; ++m) {
                        get_balance(node.resource_slug).consumption[m] +=
                            detail::pick(node.quantity_per_month, scenario);
                    }
                }
            },
            node_var);
    });

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

    graph.for_each_node([&](NodeId /*id*/, const NodeVariant& node_var) {
        if (!std::holds_alternative<EntityInstanceNode>(node_var)) {
            return;
        }
        const auto& inst = std::get<EntityInstanceNode>(node_var);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt) {
            return;
        }
        double qty = detail::pick(inst.quantity, scenario);
        auto monthly = labor_for_instance(*entity_opt, qty, scenario);
        for (std::size_t m = 0; m < 12; ++m) {
            total[m] += monthly[m];
        }
    });

    return total;
}

// ── compute_bom ───────────────────────────────────────────────────────────────

InfrastructureBOM compute_bom(const Graph& graph, const Registry& registry, Scenario scenario) {
    InfrastructureBOM bom;
    std::unordered_map<std::string, double> materials;

    graph.for_each_node([&](NodeId /*id*/, const NodeVariant& node_var) {
        if (!std::holds_alternative<EntityInstanceNode>(node_var)) {
            return;
        }
        const auto& inst = std::get<EntityInstanceNode>(node_var);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt) {
            return;
        }
        double qty = detail::pick(inst.quantity, scenario);
        const auto& infra = entity_opt->infrastructure;

        bom.total_area_m2 += detail::pick(infra.area_m2, scenario) * qty;
        bom.estimated_initial_cost += detail::pick(infra.initial_cost, scenario) * qty;
        bom.initial_labor_hours += detail::pick(infra.initial_labor_hours, scenario) * qty;

        for (const auto& mat : infra.construction_materials) {
            materials[mat.resource_slug] += detail::pick(mat.quantity_per_cycle, scenario) * qty;
        }
    });

    bom.materials.reserve(materials.size());
    for (auto& [slug, qty] : materials) {
        bom.materials.emplace_back(slug, qty);
    }
    return bom;
}

// ── compute_loop_closure_score ────────────────────────────────────────────────

double compute_loop_closure_score(const std::vector<ResourceBalance>& balance_sheet,
                                  const std::unordered_map<std::string, double>& weights) {
    double total_demand = 0.0;
    double internal_supply = 0.0;

    for (const auto& bal : balance_sheet) {
        double demand = bal.annual_consumption;
        if (demand <= 0.0) {
            continue;
        }
        double w = weights.contains(bal.resource_slug) ? weights.at(bal.resource_slug) : 1.0;
        total_demand += w * demand;
        internal_supply += w * std::min(bal.annual_internal_production, demand);
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
    // Pass empty weight map: all weights default to 1.0, preserving existing behaviour.
    // Feature 003 will inject a weight map via SolverConfig.
    result.loop_closure_score = compute_loop_closure_score(result.balance_sheet, {});

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

    // Nutrient balance (post-solve, read-only — does not modify graph or quantities).
    result.nutrient_balance =
        compute_nutrient_balance(result.graph, registry, result.balance_sheet, config.scenario);

    // Nutrient deficit diagnostics.
    check_nutrient_deficits(result.nutrient_balance, result.diagnostics);
}

}  // namespace homestead
