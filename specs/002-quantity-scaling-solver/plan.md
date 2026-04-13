# Implementation Plan: Quantity Scaling Solver

**Branch**: `002-quantity-scaling-solver` | **Date**: 2026-04-08 | **Spec**: [spec.md](spec.md)  
**Input**: Feature specification from `specs/002-quantity-scaling-solver/spec.md`

## Summary

Fix the backpropagation solver to compute meaningful entity instance quantities from
production goals, instead of hardcoding every instance to `quantity = 1.0`. The solver
must propagate scaled input demands upstream and iterate until quantities stabilize
(fixed-point convergence). Additionally: extract the duplicated `pick()` helper, replace
the fragile node-iteration pattern in analytics, and add a weight parameter to the loop
closure score for future economic weighting.

## Technical Context

**Language/Version**: C++23 (GCC 13+, Clang 17+, MSVC latest)  
**Primary Dependencies**: nlohmann/json 3.11+, Catch2 v3.6+ (testing), spdlog 1.13+ (optional logging) — all via FetchContent  
**Storage**: N/A (in-memory graph; no persistence changes in this feature)  
**Testing**: Catch2 v3, `ctest --preset debug`  
**Target Platform**: Linux / macOS / Windows (same matrix as feature 001)  
**Project Type**: C++ library (`homestead::solver`, `homestead::graph` targets modified)  
**Performance Goals**: Solver must handle 10,000-node graphs in under 1 second (Constitution §VIII); quantity convergence loop adds at most 50 extra iterations  
**Constraints**: No new external dependencies; no new CMake targets; all public API signatures unchanged  
**Scale/Scope**: Default registry (~20 entities); integration tests use hand-calculated scenarios

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Modern C++23 Idioms | ✓ PASS | `std::expected`, `std::span`, concepts, ranges used throughout; no raw new/delete; no exceptions |
| II. Separation of Concerns | ✓ PASS | Changes confined to `homestead::solver` and `homestead::graph`; `homestead::core` untouched; no I/O in domain types |
| III. Testing (NON-NEGOTIABLE) | ✓ PASS | New test file `test_quantity_scaling.cpp`; existing tests updated; integration tests use hand-calculated expected values; property-based invariant test added |
| IV. Documentation | ✓ PASS | `Graph::for_each_node` gets Doxygen comment; internal `pick.hpp` has inline comments; no other public header changes |
| V. Code Style | ✓ PASS | snake_case / PascalCase / UPPER_SNAKE conventions maintained; clang-format and clang-tidy must pass |
| VI. Build System | ✓ PASS | No new targets; `test_quantity_scaling.cpp` added to existing `test_solver` executable in `tests/solver/CMakeLists.txt` |
| VII. Continuous Integration | ✓ PASS | Existing CI matrix unchanged; no new jobs required |
| VIII. Performance | ✓ PASS | Convergence cap = 50 iterations; quantity loop is O(nodes × iterations) = within the 10k-node/1s target |

No violations. No Complexity Tracking entries required.

## Project Structure

### Documentation (this feature)

```text
specs/002-quantity-scaling-solver/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (public API delta)
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code Changes

```text
src/solver/
├── detail/
│   └── pick.hpp                   # NEW — shared pick(VariableQuantity, Scenario) helper
├── backpropagation.cpp            # MODIFIED — quantity computation + upstream scaling
└── analytics.cpp                  # MODIFIED — for_each_node, shared pick, weighted score
# convergence.cpp — NOT modified (no pick() usage; accesses .expected directly)

include/homestead/graph/
└── graph.hpp                      # MODIFIED — add Graph::for_each_node<Callable>

tests/solver/
├── test_backpropagation.cpp       # MODIFIED — update quantity assertions, add scenarios
├── test_quantity_scaling.cpp      # NEW — dedicated quantity scaling test suite
└── CMakeLists.txt                 # MODIFIED — add test_quantity_scaling.cpp source
```

---

## Phase 0: Research

### R1 — Hand-calculated baseline for integration tests

**broiler_chicken** (from `default_registry_data.cpp`):
- Output: `broiler_meat_kg`, `quantity_per_cycle = VariableQuantity{1.5, 2.0, 2.5}` → expected = **2.0 kg/cycle**
- Lifecycle: `cycles_per_year = 6.0`, `active_months = ALL_MONTHS (12)`
- `cycles_in_month(lc, m) = 6.0 / 12 = 0.5` for any active month
- **output_per_month = 2.0 × 0.5 = 1.0 kg/month per instance**
- Goal: 50 kg/month → `required = ceil(50.0 / 1.0) = 50 instances`

**Upstream demand at 50 instances** (expected scenario):
- `poultry_feed_kg`: 4.0 kg/cycle × 0.5 cycles/month × 50 = **100.0 kg/month**
- `fresh_water_l`: 10.0 L/cycle × 0.5 cycles/month × 50 = **250.0 L/month**
- `human_labor_hours`: 0.3 h/cycle × 0.5 cycles/month × 50 = **7.5 h/month**

These values are the hand-calculated expectations for SC-001 and the integration test in `test_quantity_scaling.cpp`.

### R2 — Convergence correctness for circular graphs

The existing Gauss-Seidel outer loop (backpropagation.cpp:133–301) iterates until `unsatisfied_inputs()` is empty or the iteration cap is reached. Quantity convergence extends this: after computing quantities in each pass, we compare the new quantity values to the previous pass. Convergence requires both:

1. No unsatisfied inputs remain, AND
2. No entity quantity changed between the current and previous pass.

Since quantities are integers (from clarification Q1), "no change" means exact equality. The iteration cap of 50 (clarification Q2) applies to the combined topology + quantity loop.

**Decision**: The quantity comparison is done inside the existing Gauss-Seidel loop by storing a `std::unordered_map<NodeId, double>` snapshot of entity quantities at the start of each outer iteration and comparing after demand processing completes.

### R3 — `Graph::for_each_node` design

The current pattern `for (NodeId nid = 0; nid < graph.node_count() + 100; ++nid)` is fragile:
- After `remove_node()`, node IDs are no longer contiguous.
- The `+100` slop is arbitrary and could miss nodes after large removals.

The fix: expose the internal node map's valid keys via a `for_each_node` template method:

```cpp
/// Invokes fn(NodeId, const NodeVariant&) for every valid node in the graph.
/// Iteration order is unspecified.
template<std::invocable<NodeId, const NodeVariant&> Callable>
void for_each_node(Callable&& fn) const {
    for (const auto& [id, node] : nodes_) {
        fn(id, node);
    }
}
```

This is added to the **public** `Graph` class in `graph.hpp`. It does not change the ABI (no virtual methods, no removed symbols). It is guarded by a concept (`std::invocable`) per Constitution §I.

### R4 — ExternalSourceNode quantity representation

`ExternalSourceNode` currently has no `quantity` field. The "quantity" of an external source is already represented by the `ResourceFlow.quantity_per_cycle` on the edge from the external source to its consumer. The fix for FR-005 is to set this edge quantity to the actual monthly shortfall, rather than the current hardcoded `VariableQuantity{1.0}`.

**Decision**: No new field on `ExternalSourceNode`. The edge `quantity_per_cycle` on flows from external sources carries the demand quantity. This keeps the `ExternalSourceNode` struct unchanged (no public API delta for node.hpp).

The monthly shortfall for a resource is:
`shortfall = goal_quantity_per_month - sum(internal_production_from_entity_flows)`

In the solver, when falling back to an external source after a failed entity allocation (area/budget exceeded), the flow quantity is set to the remaining unmet demand for that resource.

### R5 — Weighted loop closure score

Current formula in `analytics.cpp:compute_loop_closure_score`:
```
score = Σ min(internal, demand) / Σ demand
```

Updated to support per-resource weights:
```
score = Σ weight(r) × min(internal(r), demand(r)) / Σ weight(r) × demand(r)
```

The weight map is an `unordered_map<string, double>` with a default of 1.0 for missing keys. It is passed as an internal parameter to `compute_loop_closure_score` — **not** exposed in `PlanResult` or `SolverConfig` yet. `populate_plan_result` passes an empty map (= all weights 1.0). Feature 003 will add the map to `SolverConfig`.

---

## Phase 1: Design & Contracts

### Data Model

The core entity structures are **unchanged**. Quantity scaling is a solver computation concern.

#### EntityInstanceNode.quantity (existing field — semantics change)

| Field | Type | Current | After This Feature |
|---|---|---|---|
| `quantity` | `VariableQuantity` | Always `{1.0, 1.0, 1.0}` | `ceil(demand / output_per_cycle_per_month)` for all three scenario points |

The `min`, `expected`, and `max` fields of `EntityInstanceNode.quantity` are all set to the same integer-valued `double` computed for the configured scenario. The triple is preserved for API compatibility.

#### ResourceFlow.quantity_per_cycle (existing field — semantics fixed for external flows)

Flows from `ExternalSourceNode` nodes are currently set to `VariableQuantity{1.0}`. After this feature, they carry the actual monthly shortfall demand.

#### New internal type: `src/solver/detail/pick.hpp`

```cpp
// Internal to homestead::solver — not part of the public API.
#pragma once
#include <homestead/core/quantity.hpp>
#include <homestead/solver/config.hpp>

namespace homestead::detail {

[[nodiscard]] inline double pick(const VariableQuantity& q, Scenario s) noexcept {
    switch (s) {
        case Scenario::optimistic:  return q.min;
        case Scenario::expected:    return q.expected;
        case Scenario::pessimistic: return q.max;
    }
    return q.expected;
}

} // namespace homestead::detail
```

Both `backpropagation.cpp` and `analytics.cpp` include this header and use `detail::pick()`. Their existing anonymous-namespace `pick()` functions are removed.

### Public API Delta

Only one public API addition:

**`include/homestead/graph/graph.hpp`** — new method:
```cpp
/// Invokes fn(NodeId, const NodeVariant&) for every valid node in the graph.
/// Iteration order is unspecified.
template<std::invocable<NodeId, const NodeVariant&> Callable>
void for_each_node(Callable&& fn) const;
```

All other changes are internal to `src/solver/`.

### Contracts

**Library contract (caller perspective)**: After this feature, calling `solve(goals, registry, config)` returns a `PlanResult` where:
- Every `EntityInstanceNode` in the graph has `quantity.expected > 0` and equals `ceil(monthly_demand / output_per_cycle_per_month)`.
- Every `ResourceFlow` from an `ExternalSourceNode` has `quantity_per_cycle.expected` equal to the monthly shortfall for that resource.
- `loop_closure_score` reflects actual scaled flows (not placeholder unit quantities).
- The graph's `node_count()` and `edge_count()` remain accurate.
- All existing `PlanResult` fields are populated as before.

**Backward compatibility**: All callers passing a `SolverConfig{}` default will see numerically correct results instead of placeholder `1.0` quantities. Tests asserting on specific quantities need updating.

### Quickstart

A worked example (also serves as the integration test scenario):

```cpp
// Compute a Sisteminha-scale plan: 50 kg broiler meat/month.
homestead::Registry reg = homestead::Registry::load_defaults();
homestead::SolverConfig cfg;  // default: expected scenario, no constraints

std::vector<homestead::ProductionGoal> goals = {
    {"broiler_meat_kg", homestead::VariableQuantity{50.0}}
};

homestead::PlanResult plan = homestead::solve(goals, reg, cfg);

// Expected: ~50 broiler_chicken instances (50 kg / 1.0 kg·instance⁻¹·month⁻¹)
// Expected: ~100 kg/month poultry_feed_kg demand
// Expected: loop_closure_score > 0 once feed producers are included
```

### Agent Context Update

After completing Phase 1:
<br>Run: `.specify/scripts/bash/update-agent-context.sh claude`

---

## Algorithm: Quantity Scaling in `backpropagation.cpp`

The key change to the solver algorithm is in the demand processing loop. For each new `EntityInstanceNode` created:

1. **Find the relevant output flow spec** for the demanded resource.
2. **Compute cycles_in_month** for a representative month: walk forward from month 0 to find the first active month in the entity's `active_months` mask. If no active month exists, `output_per_month = 0` and the entity is skipped (demand falls to external source).
3. **Compute required quantity**:
   ```
   output_per_month = pick(output.quantity_per_cycle, scenario) × cycles_in_month(lc, representative_month)
   required = (output_per_month > 0) ? ceil(demand_per_month / output_per_month) : 0
   ```
   Where `demand_per_month = pick(consumer_goal.quantity_per_month, scenario)` for a GoalSinkNode consumer, or the scaled input demand from the downstream entity.
4. **Set `inst.quantity = VariableQuantity{static_cast<double>(required)}`** (all three triple points set to the same value).
5. **Scale input demands**: when enqueueing inputs for the new entity, compute `scaled_demand = pick(inp.quantity_per_cycle, scenario) × required` and pass it alongside the resource slug.

For existing entity nodes that need a quantity increase (because a second consumer added demand):
- **Do NOT mutate the node via `Graph::get_node()`.** That method returns a by-value copy (`std::optional<NodeVariant>`); mutations have no effect on the stored node.
- Instead, maintain a solver-local map `std::unordered_map<std::string, double> entity_required_qty` (keyed by entity slug) that accumulates total demand across all consumers.
- When a reuse path arrives with additional demand, add to `entity_required_qty[slug]` and recompute `ceil(total / output_per_month)`.
- If the new required quantity exceeds the value in the map (i.e., this iteration increased it), re-enqueue the entity's inputs with the incremented delta and mark the outer iteration as changed.
- At `EntityInstanceNode` creation time, look up `entity_required_qty[slug]` (defaulting to the current demand) and write the final quantity into `inst.quantity`. Nodes are written once and never mutated after insertion.

### Demand tracking structure

The current `Demand` struct carries only `(consumer_id, resource_slug)`. It must be extended to carry the **quantity being demanded**:

```cpp
struct Demand {
    NodeId consumer_id;
    std::string resource_slug;
    double quantity_per_month;  // NEW — scaled demand from consumer
};
```

Goal sinks seed the demand queue with `goal.quantity_per_month`. When an entity's inputs are enqueued, the demand is `pick(inp.quantity_per_cycle, scenario) × entity_quantity × cycles_in_month`.

---

## Complexity Tracking

No constitution violations. No entries required.
