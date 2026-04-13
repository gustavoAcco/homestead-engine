# Tasks: Quantity Scaling Solver

**Input**: Design documents from `specs/002-quantity-scaling-solver/`  
**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, contracts/ ✓, quickstart.md ✓

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- Exact file paths included in all descriptions

---

## Phase 1: Setup

**Purpose**: Create the one new file and register the new test source — no logic yet.

- [X] T001 Create `src/solver/detail/pick.hpp` with `homestead::detail::pick(const VariableQuantity&, Scenario) noexcept -> double` extracted verbatim from the anonymous-namespace `pick()` in `src/solver/backpropagation.cpp` (include guard, `#pragma once`, namespace `homestead::detail`)
- [X] T002 [P] Create empty skeleton `tests/solver/test_quantity_scaling.cpp` with `#include` boilerplate and one placeholder `TEST_CASE` that does `SUCCEED()`, then add `test_quantity_scaling.cpp` to the source list in `tests/solver/CMakeLists.txt`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared refactors that all three user stories depend on. No logic changes yet — only reorganisation and the new Graph API.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete and `ctest --preset debug` still passes.

- [X] T003 [P] Add `Graph::for_each_node` template method to `include/homestead/graph/graph.hpp`: concept-constrained signature `template<std::invocable<NodeId, const NodeVariant&> Callable> void for_each_node(Callable&&) const;` implemented inline iterating `nodes_`; add Doxygen `///` comment per Constitution §IV
- [X] T004 [P] Refactor `src/solver/analytics.cpp`: (a) remove the anonymous-namespace `pick()` function, add `#include "detail/pick.hpp"`, replace all three `pick(` call sites with `detail::pick(`; (b) replace all three fragile `for (NodeId nid = 0;; ++nid) { if (nid >= node_count()+100) break; }` loops in `compute_balance_sheet`, `compute_labor_schedule`, and `compute_bom` with `graph.for_each_node(...)` lambdas — depends on T001, T003
- [X] T005 Refactor `src/solver/backpropagation.cpp`: (a) remove the anonymous-namespace `pick()` function, add `#include "detail/pick.hpp"`, replace all `pick(` call sites with `detail::pick(`; (b) add field `double quantity_per_month{0.0}` to the internal `Demand` struct; (c) when seeding goal-sink demands in the Step 1 loop, set `demand.quantity_per_month = detail::pick(goal.quantity_per_month, config.scenario)` — depends on T001
- [X] T006 Build and run full test suite to confirm zero regressions: `cmake --build --preset debug && ctest --preset debug` — depends on T003, T004, T005

**Checkpoint**: All 98+ existing tests pass. Foundation ready for user-story implementation.

---

## Phase 3: User Story 1 — Goal-Driven Entity Counts (Priority: P1) 🎯 MVP

**Goal**: When the solver creates a new `EntityInstanceNode`, compute its required quantity from the goal demand instead of hardcoding `1.0`. External source flow quantities reflect the actual monthly shortfall.

**Independent Test**: `ctest --preset debug -R test_solver` with new integration test asserting 50 broiler_chicken instances (±5%) for a 50 kg/month broiler_meat_kg goal.

- [X] T007 [US1] In `src/solver/backpropagation.cpp`, when creating a new `EntityInstanceNode` (line ~233): (a) find the matching output `ResourceFlowSpec` for `resource_slug`; (b) compute `output_per_month = detail::pick(out.quantity_per_cycle, config.scenario) × cycles_in_month(entity_ref.lifecycle, representative_month)`; (c) guard against zero (skip entity → fall through to external); (d) set `inst.quantity = VariableQuantity{std::ceil(demand.quantity_per_month / output_per_month)}` — depends on T005
- [X] T008 [US1] In `src/solver/backpropagation.cpp`, update all three `flow.quantity_per_cycle = VariableQuantity{1.0}` assignments for `ExternalSourceNode` flows (area-exceeded path, budget-exceeded path, and no-producer path) to use `VariableQuantity{demand.quantity_per_month}` instead — depends on T005
- [X] T009 [P] [US1] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("Single goal: 50 kg broiler_meat_kg/month yields ~50 broiler_chicken instances")`: load default registry, solve with goal `{"broiler_meat_kg", 50.0}`, iterate nodes with `for_each_node`, find `EntityInstanceNode` with `entity_slug == "broiler_chicken"`, assert `quantity.expected` is within 5% of 50.0 — depends on T007
- [X] T010 [P] [US1] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("No producer: external source flow carries actual demand quantity")`: build a minimal `Registry` with one resource and no producer, solve with a 10.0 kg/month goal, assert the `ExternalSourceNode` flow has `quantity_per_cycle.expected == 10.0` — depends on T008
- [X] T010b [P] [US1] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("Single goal: 200 eggs/month yields correct laying_hen quantity")`: load default registry, solve with goal `{"egg", 200.0}`, find `EntityInstanceNode` with `entity_slug == "laying_hen"`, assert `quantity.expected * output_per_month >= 200.0` — covers US1 acceptance scenario 2 — depends on T007
- [X] T010c [P] [US1] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("Zero goal quantity produces no entity instances")`: solve with `ProductionGoal{"broiler_meat_kg", VariableQuantity{0.0}}`, assert no `EntityInstanceNode` exists in the graph (only the goal sink node) — depends on T007
- [X] T011 [US1] In `tests/solver/test_backpropagation.cpp`, review all assertions: the `has_tilapia` test checks node existence only (still passes); if any test asserts `quantity.expected == 1.0` explicitly, update to use the correct computed value; run `ctest --preset debug -R test_solver` — depends on T007, T008, T009, T010, T010b, T010c

**Checkpoint**: User Story 1 fully functional. `solve({{"broiler_meat_kg", {50.0}}}, reg, {})` returns ~50 instances.

---

## Phase 4: User Story 2 — Upstream Input Scaling (Priority: P2)

**Goal**: When an entity is instantiated with quantity N, its input demands are scaled by N and propagate upstream, driving the quantities of all upstream entities. The fixed-point loop converges on stable quantities.

**Independent Test**: `ctest --preset debug -R test_solver` with new chain test asserting ~100 kg/month `poultry_feed_kg` demand when 50 broiler_chicken instances are required.

- [X] T012 [US2] In `src/solver/backpropagation.cpp`, in the "Enqueue all of this entity's declared inputs" block (line ~244): replace the bare `demands.push(Demand{entity_id, inp.resource_slug})` with scaled demand — compute `scaled = detail::pick(inp.quantity_per_cycle, config.scenario) × entity_quantity × cycles_in_month(entity_ref.lifecycle, representative_month)` and set `Demand{entity_id, inp.resource_slug, scaled}` — depends on T007
- [X] T013 [US2] In `src/solver/backpropagation.cpp`, add a solver-local `std::unordered_map<std::string, double> entity_required_qty` (entity slug → accumulated demand). In the "reuse existing entity node" path (line ~186): add `demand.quantity_per_month` to `entity_required_qty[entity_slug_sel]`; recompute `new_qty = ceil(total / output_per_month)`; if `new_qty > entity_required_qty` previously stored, re-enqueue all of the entity's inputs with re-scaled demands using the incremented delta, and flag the iteration as changed. At new-node creation time (line ~233), initialise `entity_required_qty[slug] = demand.quantity_per_month` and use it for `inst.quantity`. Nodes are written once and never mutated after `add_entity_instance()` — `graph.get_node()` returns a copy and must not be used for mutations — depends on T012
- [X] T014 [US2] Add `int max_quantity_iterations{50}` field to `include/homestead/solver/config.hpp` (do NOT modify the existing `max_convergence_iterations{100}` default — that would silently break existing callers). In `src/solver/backpropagation.cpp`, in the Gauss-Seidel outer loop: before each outer iteration, snapshot entity quantities from `entity_required_qty` into `prev_quantities`; after demand processing completes, compare current values to snapshot; if all equal AND `unsatisfied_inputs()` is empty, break early; otherwise continue up to `config.max_quantity_iterations` before emitting a `non_convergent_cycle` diagnostic — depends on T013
- [X] T015 [P] [US2] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("Chain: 50 broiler_chicken instances drive ~100 kg/month poultry_feed demand")`: solve 50 kg broiler_meat_kg goal, find the poultry_feed producer entity, assert its quantity satisfies `quantity × output_per_month ≥ 100.0` (within 5%) — depends on T012
- [X] T016 [P] [US2] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("Circular dependency converges: chicken manure loop stabilises")`: build a small Registry with a 3-entity cycle (A produces X which feeds B which produces Y which feeds A), solve with a demand for A's primary output; assert the solver converges (no `non_convergent_cycle` diagnostic) and that all entity quantities are ≥ 1 — depends on T014
- [X] T017 [US2] Build and verify: `ctest --preset debug -R test_solver` — all tests pass — depends on T014, T015, T016

**Checkpoint**: User Stories 1 AND 2 both functional. Upstream demand fully propagated.

---

## Phase 5: User Story 3 — Accurate Self-Sufficiency Score (Priority: P3)

**Goal**: The loop closure score reflects actual scaled resource flows with a weight-map infrastructure (all weights default to 1.0, preserving current behaviour).

**Independent Test**: `ctest --preset debug -R test_solver` with assertions that a fully-internal plan scores 1.0 and a fully-external plan scores 0.0.

- [X] T018 [US3] In `src/solver/analytics.cpp`, update `compute_loop_closure_score` signature to `double compute_loop_closure_score(const std::vector<ResourceBalance>&, const std::unordered_map<std::string, double>& weights = {})` and update the formula to `Σ w(r)·min(internal(r), demand(r)) / Σ w(r)·demand(r)` where `w(r) = weights.count(r) ? weights.at(r) : 1.0` — depends on T004
- [X] T019 [US3] In `src/solver/analytics.cpp`, update the `populate_plan_result` call to `compute_loop_closure_score` to pass an empty `{}` weight map (no functional change; enables feature 003 to inject weights later) — depends on T018
- [X] T020 [P] [US3] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("Loop closure score: fully internal plan scores 1.0")`: build a Registry where resource A is produced by entity X which consumes only resource B, and B is produced by entity Y with no inputs; solve with a goal for A; assert `plan.loop_closure_score` ≈ 1.0 (within 1e-9) — depends on T018
- [X] T021 [P] [US3] In `tests/solver/test_quantity_scaling.cpp`, add `TEST_CASE("Property: incoming flow sum >= goal demand for every goal sink")`: solve with the full default registry Sisteminha goals (`data/sisteminha_goals.json`); for each `GoalSinkNode` in the graph, sum `quantity_per_cycle.expected` of all incoming flows and assert the sum ≥ `sink.quantity_per_month.expected` — depends on T018
- [X] T022 [US3] Build and verify all solver tests: `ctest --preset debug -R test_solver` — depends on T019, T020, T021

**Checkpoint**: All three user stories fully functional. Full test suite green.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Code quality, tooling verification, and end-to-end validation.

- [X] T023 [P] Run clang-format-17 on all modified and new files: `clang-format-17 -i include/homestead/graph/graph.hpp src/solver/detail/pick.hpp src/solver/backpropagation.cpp src/solver/analytics.cpp tests/solver/test_quantity_scaling.cpp tests/solver/test_backpropagation.cpp`; commit if any changes
- [X] T024 [P] Run clang-tidy-17 on new/modified headers: `clang-tidy-17 --extra-arg="-stdlib=libc++" include/homestead/graph/graph.hpp src/solver/detail/pick.hpp` — fix any reported violations in those files — depends on T023
- [X] T025 Run full test suite across all presets: `ctest --preset debug && cmake --build --preset sanitize && ctest --preset sanitize` — all tests pass with no sanitizer errors — depends on T023
- [X] T026 CLI end-to-end validation: `cmake --build --preset debug && ./build/debug/src/cli/homestead_cli --defaults --goals data/sisteminha_goals.json --output plan.json`; inspect `plan.json` and verify at least one entity has `quantity.expected > 1` and `loop_closure_score` is between 0 and 1 — depends on T025

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately; T001 and T002 are parallel
- **Phase 2 (Foundational)**: T003 and T004/T005 partially parallel; T006 gates entry to user stories
- **Phase 3 (US1)**: Requires Phase 2 complete; T009 and T010 parallel, T011 gates Phase 4
- **Phase 4 (US2)**: Requires Phase 3 complete; T015 and T016 parallel, T017 gates Phase 5
- **Phase 5 (US3)**: Requires Phase 4 complete; T020 and T021 parallel, T022 gates Phase 6
- **Phase 6 (Polish)**: Requires all user stories complete; T023 and T024 parallel

### Dependency Graph

```
T001 ──┬──► T004 (analytics refactor)
       │
T001 ──┴──► T005 (backprop refactor)
T002        (independent)
T003 ──────► T004
T004, T005, T003 ──► T006 (green gate)
T006 ──► T007 (quantity compute)
         T007 ──► T008 ──► T011
         T007 ──► T009 ──► T011
         T008 ──► T010 ──► T011
T011 ──► T012 ──► T013 ──► T014 ──► T017
                  T012 ──► T015 ──► T017
                  T014 ──► T016 ──► T017
T017 ──► T018 ──► T019 ──► T022
         T018 ──► T020 ──► T022
         T018 ──► T021 ──► T022
T022 ──► T023 ──► T024 ──► T025 ──► T026
```

### Parallel Opportunities per Phase

| Phase | Parallel tasks | Sequential bottleneck |
|---|---|---|
| Setup | T001 ∥ T002 | — |
| Foundational | T003 ∥ T005; T004 after T003 | T006 gates all |
| US1 | T009 ∥ T010 | T007→T008→T011 |
| US2 | T015 ∥ T016 | T012→T013→T014→T017 |
| US3 | T020 ∥ T021 | T018→T019→T022 |
| Polish | T023 ∥ T024 | T025→T026 |

---

## Implementation Strategy

### MVP First (User Story 1 only)

1. Complete Phase 1 + Phase 2 (setup + foundational refactors)
2. Complete Phase 3 (US1 — correct entity quantities + external source quantities)
3. **STOP and VALIDATE**: `ctest --preset debug -R test_solver` — 50-instance integration test passes
4. Run CLI: `homestead_cli --defaults --goals data/sisteminha_goals.json --output plan.json` — non-trivial quantities visible

### Incremental Delivery

1. Phase 1 + 2 → foundation (zero regressions)
2. Phase 3 → correct entity counts → MVP
3. Phase 4 → full upstream propagation → quantities throughout the chain are correct
4. Phase 5 → meaningful loop closure score → planning metric is usable
5. Phase 6 → clean, linted, sanitizer-verified code → ready to merge

---

## Notes

- All 29 tasks follow the strict checklist format (26 original + T010b, T010c for missing acceptance scenarios, and updated T013/T014 for C1/I2 fixes)
- `[P]` tasks operate on different files or are independent sub-tests
- The foundational phase (T003–T006) is a prerequisite gate; do not skip it
- Hand-calculated expected values in the quickstart (50 instances, 100 kg/month feed demand) serve as ground truth for integration tests
- T014 adds NEW `config.max_quantity_iterations{50}` to `include/homestead/solver/config.hpp`; existing `max_convergence_iterations{100}` is NOT changed (I2 fix)
- `cycles_in_month` for the "representative month" calculation: use the first active month (walk from month 0); if no active months, skip entity (→ external source)
