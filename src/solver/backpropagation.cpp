// Backpropagation solver: demand-driven backward expansion with Gauss-Seidel
// fixed-point convergence for circular dependencies (research.md §1–2).
//
// Algorithm:
//   1. Seed the demand queue with (goal_sink_id, resource_slug) for every goal.
//   2. Pop a demand.  If an entity node for the required resource already exists
//      in the graph, connect it and continue.  Otherwise:
//        a. Find producers from the registry.
//        b. Select the best candidate (highest loop-closure contribution).
//        c. Check constraints (area, budget).
//        d. Add EntityInstanceNode + ResourceFlow.
//        e. Enqueue all of the new entity's declared inputs as new demands.
//   3. If no registry producer exists, add an ExternalSourceNode.
//   4. After processing all demands, recheck unsatisfied_inputs() as the
//      Gauss-Seidel outer loop.  Repeat until convergence or iteration cap.
//   5. Run analytics and build PlanResult.

#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <queue>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "detail/pick.hpp"

namespace homestead {

// Defined in analytics.cpp.
void populate_plan_result(PlanResult&, const std::vector<ProductionGoal>&, const Registry&,
                          const SolverConfig&);

// Defined in scheduling.cpp.
double cycles_in_month(const Lifecycle& lc, int month) noexcept;

namespace {

/// Select the entity most likely to close loops.
/// Returns the slug of the best candidate, or empty string if none found.
std::string select_entity_slug(const std::vector<std::string>& slugs, const Registry& registry) {
    std::string best_slug;
    std::size_t best_sz = 0;
    for (const auto& slug : slugs) {
        auto opt = registry.find_entity(slug);
        if (!opt) {
            continue;
        }
        if (best_slug.empty() || opt->outputs.size() > best_sz) {
            best_slug = slug;
            best_sz = opt->outputs.size();
        }
    }
    return best_slug;
}

struct Demand {
    NodeId consumer_id;
    std::string resource_slug;
    double quantity_per_month{0.0};  // scaled demand from consumer
};

/// Get or create an ExternalSourceNode for resource_slug; return its NodeId.
NodeId get_or_create_external(const std::string& resource_slug, Graph& g,
                              std::unordered_map<std::string, NodeId>& external_nodes) {
    auto eit = external_nodes.find(resource_slug);
    if (eit != external_nodes.end()) {
        return eit->second;
    }
    ExternalSourceNode esn;
    esn.resource_slug = resource_slug;
    esn.cost_per_unit = 0.0;
    NodeId src_id = g.add_external_source(esn);
    external_nodes[resource_slug] = src_id;
    return src_id;
}

/// Find the first active month in the lifecycle (walk from month 0).
/// Returns -1 if no active month exists.
int representative_month(const Lifecycle& lc) noexcept {
    for (int m = 0; m < 12; ++m) {
        if (is_active(lc.active_months, m)) {
            return m;
        }
    }
    return -1;
}

}  // anonymous namespace

// ── BackpropagationSolver::solve ───────────────────────────────────────────────

PlanResult BackpropagationSolver::solve(std::span<const ProductionGoal> goals,
                                        const Registry& registry, const SolverConfig& config) {
    PlanResult result;
    Graph& g = result.graph;

    if (goals.empty()) {
        return result;
    }

    // Track entity-instance nodes by entity slug to reuse them.
    std::unordered_map<std::string, NodeId> entity_nodes;
    // Track external-source nodes by resource slug.
    std::unordered_map<std::string, NodeId> external_nodes;
    // Track which (consumer, resource) pairs we have already satisfied.
    std::unordered_set<std::string> satisfied_keys;  // "consumer_id:resource_slug"
    // Solver-local accumulated demand per entity slug (slug → total monthly demand).
    // Nodes are created once and never mutated — all quantity updates happen here.
    std::unordered_map<std::string, double> entity_required_qty;

    double total_area = 0.0;
    double total_cost = 0.0;

    auto make_key = [](NodeId id, const std::string& slug) {
        return std::format("{}:{}", id, slug);
    };

    // ── Step 1: Create goal sinks and seed demand queue ───────────────────────
    std::queue<Demand> demands;

    for (const auto& goal : goals) {
        if (!registry.find_resource(goal.resource_slug)) {
            result.diagnostics.push_back(
                Diagnostic{DiagnosticKind::missing_producer,
                           std::format("Goal resource '{}' not in registry", goal.resource_slug),
                           goal.resource_slug, std::nullopt});
            continue;
        }
        GoalSinkNode sink;
        sink.resource_slug = goal.resource_slug;
        sink.quantity_per_month = goal.quantity_per_month;
        NodeId sink_id = g.add_goal_sink(sink);
        double qty_per_month = detail::pick(goal.quantity_per_month, config.scenario);
        // Skip zero-quantity goals — goal sink is recorded but no demand propagated.
        if (qty_per_month > 0.0) {
            demands.push(Demand{sink_id, goal.resource_slug, qty_per_month});
        }
    }

    // ── Step 2–4: Fixed-point backward expansion ───────────────────────────────
    for (int iter = 0; iter < config.max_convergence_iterations; ++iter) {
        if (demands.empty()) {
            break;
        }

        // Snapshot entity quantities for convergence detection.
        std::unordered_map<std::string, double> prev_quantities = entity_required_qty;

        // Process all current demands.
        std::size_t processed = 0;
        while (!demands.empty()) {
            auto [consumer_id, resource_slug, qty_per_month] = demands.front();
            demands.pop();

            std::string key = make_key(consumer_id, resource_slug);
            if (satisfied_keys.contains(key)) {
                continue;
            }

            // ── Find or reuse a producer ───────────────────────────────────────
            auto producer_slugs = registry.producers_of(resource_slug);

            if (producer_slugs.empty()) {
                // No producer — use/create an ExternalSourceNode.
                NodeId src_id = get_or_create_external(resource_slug, g, external_nodes);
                // Only emit the diagnostic once (when node was just created).
                if (external_nodes.find(resource_slug) != external_nodes.end() &&
                    external_nodes[resource_slug] == src_id) {
                    result.diagnostics.push_back(Diagnostic{
                        DiagnosticKind::missing_producer,
                        std::format("No producer for '{}' in registry; purchasing externally",
                                    resource_slug),
                        resource_slug, std::nullopt});
                }
                ResourceFlow flow;
                flow.from = src_id;
                flow.to = consumer_id;
                flow.resource_slug = resource_slug;
                flow.quantity_per_cycle =
                    VariableQuantity{qty_per_month > 0.0 ? qty_per_month : 1.0};
                (void)g.add_flow(flow);
                satisfied_keys.insert(key);
                ++processed;
                continue;
            }

            std::string entity_slug_sel = select_entity_slug(producer_slugs, registry);
            if (entity_slug_sel.empty()) {
                continue;
            }
            auto entity_opt = registry.find_entity(entity_slug_sel);
            if (!entity_opt) {
                continue;
            }
            const Entity& entity_ref = *entity_opt;

            // ── Reuse existing entity node if present ──────────────────────────
            NodeId entity_id = INVALID_NODE;
            auto nit = entity_nodes.find(entity_slug_sel);
            if (nit != entity_nodes.end()) {
                entity_id = nit->second;
                // Accumulate demand and re-enqueue inputs if required quantity increases.
                double old_total = entity_required_qty[entity_slug_sel];
                entity_required_qty[entity_slug_sel] += qty_per_month;
                double new_total = entity_required_qty[entity_slug_sel];

                int rep_month_r = representative_month(entity_ref.lifecycle);
                double output_per_month_r = 0.0;
                if (rep_month_r >= 0) {
                    auto out_it_r = std::ranges::find_if(
                        entity_ref.outputs, [&resource_slug](const ResourceFlowSpec& o) {
                            return o.resource_slug == resource_slug;
                        });
                    if (out_it_r != entity_ref.outputs.end()) {
                        double cpm_r = cycles_in_month(entity_ref.lifecycle, rep_month_r);
                        output_per_month_r =
                            detail::pick(out_it_r->quantity_per_cycle, config.scenario) * cpm_r;
                    }
                }
                if (output_per_month_r > 0.0) {
                    double old_qty = std::ceil(old_total / output_per_month_r);
                    double new_qty = std::ceil(new_total / output_per_month_r);
                    if (new_qty > old_qty) {
                        // Re-enqueue inputs with the incremental delta demand.
                        double delta = new_qty - old_qty;
                        double cpm_r = cycles_in_month(entity_ref.lifecycle, rep_month_r);
                        for (const auto& inp : entity_ref.inputs) {
                            double scaled_delta =
                                detail::pick(inp.quantity_per_cycle, config.scenario) * delta *
                                cpm_r;
                            demands.push(Demand{entity_id, inp.resource_slug, scaled_delta});
                        }
                    }
                }
            } else {
                // Check constraints before allocating.
                double area = detail::pick(entity_ref.infrastructure.area_m2, config.scenario);
                double cost = detail::pick(entity_ref.infrastructure.initial_cost, config.scenario);

                if (config.max_area_m2 && (total_area + area) > *config.max_area_m2) {
                    result.diagnostics.push_back(
                        Diagnostic{DiagnosticKind::area_exceeded,
                                   std::format("Cannot add '{}': area constraint ({:.1f} m²) "
                                               "exceeded",
                                               entity_slug_sel, *config.max_area_m2),
                                   resource_slug, entity_slug_sel});
                    // Fall back to external source.
                    NodeId src_id = get_or_create_external(resource_slug, g, external_nodes);
                    ResourceFlow flow;
                    flow.from = src_id;
                    flow.to = consumer_id;
                    flow.resource_slug = resource_slug;
                    flow.quantity_per_cycle =
                        VariableQuantity{qty_per_month > 0.0 ? qty_per_month : 1.0};
                    (void)g.add_flow(flow);
                    satisfied_keys.insert(key);
                    ++processed;
                    continue;
                }

                if (config.max_budget && (total_cost + cost) > *config.max_budget) {
                    result.diagnostics.push_back(Diagnostic{
                        DiagnosticKind::unsatisfied_goal,
                        std::format("Cannot add '{}': budget constraint exceeded (cost={:.2f})",
                                    entity_slug_sel, cost),
                        resource_slug, entity_slug_sel});
                    NodeId src_id = get_or_create_external(resource_slug, g, external_nodes);
                    ResourceFlow flow;
                    flow.from = src_id;
                    flow.to = consumer_id;
                    flow.resource_slug = resource_slug;
                    flow.quantity_per_cycle =
                        VariableQuantity{qty_per_month > 0.0 ? qty_per_month : 1.0};
                    (void)g.add_flow(flow);
                    satisfied_keys.insert(key);
                    ++processed;
                    continue;
                }

                // Compute required quantity from demand.
                int rep_month = representative_month(entity_ref.lifecycle);
                double output_per_month = 0.0;
                if (rep_month >= 0) {
                    auto out_it = std::ranges::find_if(entity_ref.outputs,
                                                       [&resource_slug](const ResourceFlowSpec& o) {
                                                           return o.resource_slug == resource_slug;
                                                       });
                    if (out_it != entity_ref.outputs.end()) {
                        double cpm = cycles_in_month(entity_ref.lifecycle, rep_month);
                        output_per_month =
                            detail::pick(out_it->quantity_per_cycle, config.scenario) * cpm;
                    }
                }

                if (output_per_month <= 0.0) {
                    // Entity produces zero of this resource — fall through to external.
                    NodeId src_id = get_or_create_external(resource_slug, g, external_nodes);
                    ResourceFlow flow;
                    flow.from = src_id;
                    flow.to = consumer_id;
                    flow.resource_slug = resource_slug;
                    flow.quantity_per_cycle =
                        VariableQuantity{qty_per_month > 0.0 ? qty_per_month : 1.0};
                    (void)g.add_flow(flow);
                    satisfied_keys.insert(key);
                    ++processed;
                    continue;
                }

                double required_qty = std::ceil(qty_per_month / output_per_month);

                // Add new entity instance node.
                EntityInstanceNode inst;
                inst.instance_name = entity_slug_sel;
                inst.entity_slug = entity_slug_sel;
                inst.quantity = VariableQuantity{required_qty};
                inst.schedule = entity_ref.lifecycle.active_months;
                entity_id = g.add_entity_instance(inst);
                entity_nodes[entity_slug_sel] = entity_id;
                entity_required_qty[entity_slug_sel] = qty_per_month;
                total_area += area;
                total_cost += cost;

                // Enqueue all of this entity's declared inputs (scaled by quantity).
                double cpm = cycles_in_month(entity_ref.lifecycle, rep_month >= 0 ? rep_month : 0);
                for (const auto& inp : entity_ref.inputs) {
                    std::string inp_key = make_key(entity_id, inp.resource_slug);
                    if (!satisfied_keys.contains(inp_key)) {
                        double scaled = detail::pick(inp.quantity_per_cycle, config.scenario) *
                                        required_qty * cpm;
                        demands.push(Demand{entity_id, inp.resource_slug, scaled});
                    }
                }
            }

            // Connect entity → consumer.
            auto succs = g.successors(entity_id);
            bool already_linked =
                std::ranges::any_of(succs, [consumer_id](NodeId s) { return s == consumer_id; });
            if (!already_linked) {
                double qty = 0.0;
                auto out_it = std::ranges::find_if(entity_ref.outputs,
                                                   [&resource_slug](const ResourceFlowSpec& o) {
                                                       return o.resource_slug == resource_slug;
                                                   });
                if (out_it != entity_ref.outputs.end()) {
                    qty = detail::pick(out_it->quantity_per_cycle, config.scenario);
                }
                ResourceFlow flow;
                flow.from = entity_id;
                flow.to = consumer_id;
                flow.resource_slug = resource_slug;
                flow.quantity_per_cycle = VariableQuantity{qty > 0 ? qty : 1.0};
                (void)g.add_flow(flow);
            }

            satisfied_keys.insert(key);
            ++processed;
        }

        if (processed == 0) {
            break;
        }

        // Convergence check: quantities stable AND no unsatisfied inputs.
        bool quantities_stable = (entity_required_qty == prev_quantities);
        auto remaining = g.unsatisfied_inputs(registry);
        if (remaining.empty() && quantities_stable) {
            break;
        }

        for (const auto& [nid, slug] : remaining) {
            std::string key = make_key(nid, slug);
            if (!satisfied_keys.contains(key)) {
                demands.push(Demand{nid, slug, 0.0});
            }
        }

        if (iter >= config.max_quantity_iterations - 1) {
            result.diagnostics.push_back(
                Diagnostic{DiagnosticKind::non_convergent_cycle,
                           std::format("Solver reached quantity iteration cap ({})",
                                       config.max_quantity_iterations),
                           std::nullopt, std::nullopt});
        }
    }

    // ── Step 5: Analytics ─────────────────────────────────────────────────────
    // Convert goals span to vector for passing to analytics helpers.
    std::vector<ProductionGoal> goals_vec(goals.begin(), goals.end());
    populate_plan_result(result, goals_vec, registry, config);

    return result;
}

// ── Free function convenience API ──────────────────────────────────────────────

PlanResult solve(std::span<const ProductionGoal> goals, const Registry& registry,
                 const SolverConfig& config) {
    return BackpropagationSolver{}.solve(goals, registry, config);
}

}  // namespace homestead
