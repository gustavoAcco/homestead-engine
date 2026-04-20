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
double compute_loop_closure_score(const std::vector<ResourceBalance>&,
                                  const std::unordered_map<std::string, double>&);

// Defined below in this file (composition-matching passes).
void run_feed_matching_pass(PlanResult& result, const Registry& registry,
                            const SolverConfig& config,
                            std::unordered_map<std::string, NodeId>& external_nodes, Graph& g);
void run_fertilization_matching_pass(PlanResult& result, const Registry& registry,
                                     const SolverConfig& config,
                                     std::unordered_map<std::string, NodeId>& external_nodes,
                                     Graph& g);

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

    // ── Step 6: Composition-matching passes (post-slug, post-analytics) ───────
    // Pass 1: route feed resources to composition_requirements on animal/per-plant entities.
    // Pass 2: route fertilizer resources to fertilization_per_m2 on area crop entities.
    // Both passes only update composition_routed in the balance sheet and emit diagnostics;
    // they never add entity instances (isolation guarantee — FR-007).
    // SC-005: measured overhead on 20-entity plan (bench_solver, release build) < 1 ms total.
    run_feed_matching_pass(result, registry, config, external_nodes, g);
    run_fertilization_matching_pass(result, registry, config, external_nodes, g);

    // Rebuild gap_report to include any new synthetic external balance entries.
    result.gap_report.clear();
    std::ranges::copy_if(result.balance_sheet, std::back_inserter(result.gap_report),
                         [](const ResourceBalance& b) {
                             return b.annual_external_purchase > 0.0 ||
                                    b.annual_internal_production < b.annual_consumption;
                         });

    // Recompute loop closure score now that composition_routed flows are populated (FR-011).
    result.loop_closure_score = compute_loop_closure_score(result.balance_sheet, {});

    return result;
}

// ── Free function convenience API ──────────────────────────────────────────────

PlanResult solve(std::span<const ProductionGoal> goals, const Registry& registry,
                 const SolverConfig& config) {
    return BackpropagationSolver{}.solve(goals, registry, config);
}

// ── Composition-matching helpers ───────────────────────────────────────────────

namespace {

/// Get or insert a ResourceBalance entry by slug (creates empty entry if absent).
ResourceBalance& get_or_insert_balance(std::vector<ResourceBalance>& sheet,
                                       const std::string& slug) {
    auto it = std::ranges::find_if(
        sheet, [&slug](const ResourceBalance& b) { return b.resource_slug == slug; });
    if (it != sheet.end()) {
        return *it;
    }
    ResourceBalance b;
    b.resource_slug = slug;
    sheet.push_back(std::move(b));
    return sheet.back();
}

/// Sorted list of candidate resource slugs for composition matching.
/// Candidates are internal resources (annual_internal_production > 0) whose
/// Resource::composition map contains at least one required key.
/// Sorted by annual_internal_production descending (highest contribution first).
std::vector<std::string> collect_candidates(
    const std::unordered_map<std::string, double>& required_keys,
    const std::vector<ResourceBalance>& balance_sheet, const Registry& registry) {
    std::vector<std::pair<double, std::string>> scored;

    for (const auto& bal : balance_sheet) {
        if (bal.annual_internal_production <= 0.0) {
            continue;
        }
        auto res_opt = registry.find_resource(bal.resource_slug);
        if (!res_opt || res_opt->composition.empty()) {
            continue;
        }
        // Must overlap at least one required key.
        bool has_overlap = std::ranges::any_of(
            required_keys, [&](const auto& kv) { return res_opt->composition.contains(kv.first); });
        if (has_overlap) {
            scored.emplace_back(bal.annual_internal_production, bal.resource_slug);
        }
    }

    std::ranges::sort(scored, std::ranges::greater{}, &std::pair<double, std::string>::first);
    std::vector<std::string> result;
    result.reserve(scored.size());
    for (auto& [score, slug] : scored) {
        result.push_back(std::move(slug));
    }
    return result;
}

}  // namespace

// ── Pass 1: feed matching (composition_requirements) ──────────────────────────
//
// Runs after populate_plan_result(). For each EntityInstanceNode with non-empty
// composition_requirements, computes annual elemental demand and routes available
// internal resources to satisfy it. Records element flows in composition_routed.
// For unmet remainders, adds a synthetic ResourceBalance and emits composition_gap.
// NEVER calls add_entity_instance (isolation guarantee, FR-007).

void run_feed_matching_pass(PlanResult& result, const Registry& registry,
                            const SolverConfig& config,
                            std::unordered_map<std::string, NodeId>& /*external_nodes*/,
                            Graph& /*g*/) {
    // Track resource units allocated so far in this pass to avoid over-allocation.
    std::unordered_map<std::string, double> allocated;  // slug → units consumed

    result.graph.for_each_node([&](NodeId /*id*/, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv)) {
            return;
        }
        const auto& inst = std::get<EntityInstanceNode>(nv);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt || entity_opt->composition_requirements.empty()) {
            return;
        }
        const Entity& entity = *entity_opt;
        const auto& reqs = entity.composition_requirements;

        double qty = detail::pick(inst.quantity, config.scenario);
        double cycles_per_year = entity.lifecycle.cycles_per_year;

        // Annual demand per element.
        std::unordered_map<std::string, double> remaining;
        for (const auto& [key, val] : reqs) {
            remaining[key] = val * qty * cycles_per_year;
        }

        auto candidates = collect_candidates(reqs, result.balance_sheet, registry);

        for (const auto& cand_slug : candidates) {
            // Stop when all elemental demands are satisfied.
            bool all_met =
                std::ranges::all_of(remaining, [](const auto& kv) { return kv.second <= 1e-9; });
            if (all_met) {
                break;
            }

            auto res_opt = registry.find_resource(cand_slug);
            if (!res_opt) {
                continue;
            }
            const auto& comp = res_opt->composition;

            // Skip candidates that cannot contribute to any remaining demand.
            bool can_contribute = std::ranges::any_of(remaining, [&](const auto& kv) {
                return kv.second > 1e-9 && comp.contains(kv.first) && comp.at(kv.first) > 0.0;
            });
            if (!can_contribute) {
                continue;
            }

            auto& bal = get_or_insert_balance(result.balance_sheet, cand_slug);
            double already = allocated.contains(cand_slug) ? allocated.at(cand_slug) : 0.0;
            double available =
                std::max(0.0, bal.annual_internal_production - bal.annual_consumption - already);
            if (available <= 0.0) {
                continue;
            }

            // Allocate enough units to satisfy the most demanding element this candidate supplies.
            double units_needed = 0.0;
            for (const auto& [key, rem] : remaining) {
                if (rem > 1e-9 && comp.contains(key) && comp.at(key) > 0.0) {
                    units_needed = std::max(units_needed, rem / comp.at(key));
                }
            }
            double units_used = std::min(units_needed, available);

            // Credit all required keys from this allocation.
            for (auto& [key, rem] : remaining) {
                if (comp.contains(key)) {
                    double provided = units_used * comp.at(key);
                    bal.composition_routed[key] += provided;
                    rem = std::max(0.0, rem - provided);
                }
            }
            allocated[cand_slug] += units_used;
        }

        // Emit composition_gap diagnostics for unmet remainder.
        for (const auto& [key, rem] : remaining) {
            if (rem <= 1e-9) {
                continue;
            }
            std::string ext_slug = key + "_external";
            auto& ext_bal = get_or_insert_balance(result.balance_sheet, ext_slug);
            ext_bal.annual_external_purchase += rem;

            result.diagnostics.push_back(Diagnostic{
                DiagnosticKind::composition_gap,
                std::format("Entity '{}' has unmet {} requirement: {:.2f} g sourced externally",
                            inst.entity_slug, key, rem),
                ext_slug, inst.entity_slug, rem});
        }
    });
}

// ── Pass 2: fertilization matching (fertilization_per_m2) ─────────────────────
//
// For each EntityInstanceNode with non-empty fertilization_per_m2, computes
// area-scaled annual NPK demand and routes available N/P/K-bearing resources.
// Primary key: N_g. Records element flows in composition_routed.
// For unmet per-element remainders, adds synthetic ResourceBalance + composition_gap.

void run_fertilization_matching_pass(PlanResult& result, const Registry& registry,
                                     const SolverConfig& config,
                                     std::unordered_map<std::string, NodeId>& /*external_nodes*/,
                                     Graph& /*g*/) {
    std::unordered_map<std::string, double> allocated;  // slug → units consumed

    result.graph.for_each_node([&](NodeId /*id*/, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv)) {
            return;
        }
        const auto& inst = std::get<EntityInstanceNode>(nv);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt || entity_opt->fertilization_per_m2.empty()) {
            return;
        }
        const Entity& entity = *entity_opt;
        const auto& per_m2 = entity.fertilization_per_m2;

        double qty = detail::pick(inst.quantity, config.scenario);
        double area_m2 = detail::pick(entity.infrastructure.area_m2, config.scenario);
        double cycles_per_year = entity.lifecycle.cycles_per_year;

        // Annual demand per element: g/m2/cycle * m2 * instances * cycles/year.
        std::unordered_map<std::string, double> remaining;
        for (const auto& [key, val] : per_m2) {
            remaining[key] = val * area_m2 * qty * cycles_per_year;
        }

        auto candidates = collect_candidates(per_m2, result.balance_sheet, registry);

        for (const auto& cand_slug : candidates) {
            // Stop when all elemental demands are satisfied.
            bool all_met =
                std::ranges::all_of(remaining, [](const auto& kv) { return kv.second <= 1e-9; });
            if (all_met) {
                break;
            }

            auto res_opt = registry.find_resource(cand_slug);
            if (!res_opt) {
                continue;
            }
            const auto& comp = res_opt->composition;

            // Skip candidates that cannot contribute to any remaining demand.
            bool can_contribute = std::ranges::any_of(remaining, [&](const auto& kv) {
                return kv.second > 1e-9 && comp.contains(kv.first) && comp.at(kv.first) > 0.0;
            });
            if (!can_contribute) {
                continue;
            }

            auto& bal = get_or_insert_balance(result.balance_sheet, cand_slug);
            double already = allocated.contains(cand_slug) ? allocated.at(cand_slug) : 0.0;
            double available =
                std::max(0.0, bal.annual_internal_production - bal.annual_consumption - already);
            if (available <= 0.0) {
                continue;
            }

            // Allocate enough units to satisfy the most demanding element this candidate supplies.
            double units_needed = 0.0;
            for (const auto& [key, rem] : remaining) {
                if (rem > 1e-9 && comp.contains(key) && comp.at(key) > 0.0) {
                    units_needed = std::max(units_needed, rem / comp.at(key));
                }
            }
            double units_used = std::min(units_needed, available);

            for (auto& [key, rem] : remaining) {
                if (comp.contains(key)) {
                    double provided = units_used * comp.at(key);
                    bal.composition_routed[key] += provided;
                    rem = std::max(0.0, rem - provided);
                }
            }
            allocated[cand_slug] += units_used;
        }

        for (const auto& [key, rem] : remaining) {
            if (rem <= 1e-9) {
                continue;
            }
            std::string ext_slug = key + "_external";
            auto& ext_bal = get_or_insert_balance(result.balance_sheet, ext_slug);
            ext_bal.annual_external_purchase += rem;

            result.diagnostics.push_back(
                Diagnostic{DiagnosticKind::composition_gap,
                           std::format("Entity '{}' has unmet fertilization {} requirement: {:.2f} "
                                       "g sourced externally",
                                       inst.entity_slug, key, rem),
                           ext_slug, inst.entity_slug, rem});
        }
    });
}

}  // namespace homestead
