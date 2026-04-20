# Tasks: Composition-Aware Resource Matching (004)

**Input**: Design documents from `specs/004-composition-matching/`
**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, contracts/ ✓, quickstart.md ✓

**Organization**: Tasks are grouped by user story. The Foundational phase (Phase 2) is a hard
prerequisite — no user story work can begin until those tasks are complete.

**Tests**: Included — spec explicitly requires 5 test cases in `test_composition_matching.cpp`
and round-trip additions in `test_roundtrip.cpp`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete tasks in same phase)
- **[Story]**: Which user story this task belongs to
- All paths are relative to the repository root

---

## Phase 1: Setup

No new project structure is required. All targets, presets, and CMakeLists targets already exist.
New source files are added to existing CMake targets in the Foundational phase.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core data model additions that MUST be complete before any solver pass or test can
be written. All four user stories depend on these fields being present.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T001 Add `composition_requirements` and `fertilization_per_m2` fields (both `std::unordered_map<std::string, double>`, default empty) with Doxygen `///` comments to `Entity` struct in `include/homestead/core/entity.hpp`
- [X] T002 Add `composition_routed` field (`std::unordered_map<std::string, double>`, default empty) with Doxygen `///` comment to `ResourceBalance` struct in `include/homestead/solver/result.hpp`
- [X] T003 [P] Add `composition_gap` enumerator with Doxygen `///` comment to `DiagnosticKind` enum in `include/homestead/solver/result.hpp`; update `to_string(DiagnosticKind)` to return `"composition_gap"`
- [X] T004 [P] Bump `PLAN_SCHEMA_VERSION` from `{1, 1, 0}` to `{1, 2, 0}` in `include/homestead/serialization/schema_version.hpp`
- [X] T005 Add registry validation for `composition_requirements` (keys non-empty, values > 0.0) and `fertilization_per_m2` (same rules) in `src/core/registry.cpp`; return `RegistryError{RegistryErrorKind::invalid_quantity, ...}` on violation — add to the existing `register_entity` validation block
- [X] T006 Add serialization round-trip for `Entity::composition_requirements` and `Entity::fertilization_per_m2` (absent key → empty map) in `src/serialization/serialization.cpp` using existing `nlohmann::json` patterns
- [X] T007 Add serialization round-trip for `ResourceBalance::composition_routed` (absent key → empty map; 1.1.x backward compat) in `src/serialization/serialization.cpp`
- [X] T008 Add round-trip test assertions for `composition_requirements`, `fertilization_per_m2`, and `composition_routed` in `tests/serialization/test_roundtrip.cpp` — verify absent-key backward compat and non-empty maps serialize/deserialize correctly
- [X] T009 Register `tests/solver/test_composition_matching.cpp` as a new source in the `homestead_solver_tests` CMake target in `CMakeLists.txt` (or relevant test CMakeLists) and create the file with the Catch2 test scaffolding (empty TEST_CASE stubs for the 5 scenarios, pure ASCII names)

**Checkpoint**: Build with `cmake --build --preset debug` succeeds. `test_roundtrip` passes.
No solver behavior changes yet.

---

## Phase 3: User Story 1 — Animal Feed from Available Crops (Priority: P1) 🎯 MVP

**Goal**: Solver automatically routes internally-produced grain and legume resources to meet
animal entities' protein and energy requirements; unmet remainder creates external elemental
purchases and `composition_gap` diagnostics.

**Independent Test**: Run `ctest --preset debug -R test_composition_matching` — Scenario 1
(broiler protein met from corn + bean) and Scenario 4 (regression: no composition_requirements)
must pass.

### Tests for User Story 1

> **Write these test bodies first, verify they FAIL, then implement the pass.**

- [X] T010 [US1] Implement Scenario 1 test body in `tests/solver/test_composition_matching.cpp`: registry with `broiler_chicken` (composition_requirements: protein_g=720, energy_kcal=11600), `corn_plot_1m2` (produces `corn_grain_kg` with protein_g=93, energy_kcal=3580 per kg), `bean_plot_1m2` (produces `bean_kg` with protein_g=220, energy_kcal=3400 per kg); goals `broiler_meat_kg=5.0/month`; assert no `composition_gap` diagnostic with `resource_slug=="protein_g_external"`
- [X] T011 [P] [US1] Implement Scenario 4 test body in `tests/solver/test_composition_matching.cpp`: same goals as an existing passing test; strip all `composition_requirements` and `fertilization_per_m2` from the registry entities; assert `loop_closure_score`, node count, and balance sheet are identical to the baseline result

### Implementation for User Story 1

- [X] T012 [US1] Implement `run_feed_matching_pass()` as a free function in `src/solver/backpropagation.cpp` (declaration in the file's anonymous namespace or a helper header): iterate `EntityInstanceNode`s with non-empty `entity.composition_requirements`; compute scaled demand (`req[key] * qty * cpm`); collect candidate resources (graph nodes whose `Resource::composition` contains required keys); sort by loop-closure contribution proxy (output count); greedy-allocate energy_kcal first, then check protein_g coverage; for unmet remainder create `ExternalSourceNode` with slug `"{key}_external"` and emit `DiagnosticKind::composition_gap`
- [X] T013 [US1] Update `ResourceBalance::composition_routed` in `run_feed_matching_pass()` for each allocated resource (accumulate grams of each element routed through it)
- [X] T014 [US1] Update `compute_loop_closure_score()` in `src/solver/analytics.cpp`: after the existing mass-flow loop, iterate `ResourceBalance::composition_routed` entries; add gram quantities (excluding `energy_kcal`) to both `internal_supply` (min of routed and demand) and `total_demand` numerators
- [X] T015 [US1] Wire `run_feed_matching_pass()` call into `BackpropagationSolver::solve()` in `src/solver/backpropagation.cpp`, positioned after the demand-queue convergence loop and before `populate_plan_result()`; add one additional Gauss-Seidel iteration after the call

**Checkpoint**: `ctest --preset debug -R test_composition_matching` — Scenario 1 and Scenario 4
pass. All existing tests still pass (`ctest --preset debug`).

---

## Phase 4: User Story 2 — Crop Fertilization from Available Manure and Compost (Priority: P2)

**Goal**: Solver automatically routes N/P/K-bearing resources (manure, nutrient water) to area-based
and per-plant crop entities; partial coverage emits `composition_gap` diagnostics per element.

**Independent Test**: Run `ctest --preset debug -R test_composition_matching` — Scenario 2 (corn
N/P/K met from chicken manure), Scenario 3 (partial via nutrient_water_l with K_g_external gap),
and Scenario 5 (nutrient_water_l routes to lettuce without slug link) must pass.

### Tests for User Story 2

> **Write these test bodies first, verify they FAIL, then implement the pass.**

- [X] T016 [US2] Implement Scenario 2 test body in `tests/solver/test_composition_matching.cpp`: registry with `corn_plot_1m2` (fertilization_per_m2: N_g=8, P_g=3, K_g=6) and `chicken_manure_kg` with composition {N_g:15, P_g:12, K_g:8}; goals `broiler_meat_kg=5.0/month` + `corn_grain_kg=3.0/month`; assert no `composition_gap` for `corn_plot_1m2` and `composition_routed["N_g"] > 0` on `chicken_manure_kg` balance
- [X] T017 [P] [US2] Implement Scenario 3 test body in `tests/solver/test_composition_matching.cpp`: `tilapia_tank_5000l` (produces `nutrient_water_l` with composition {N_g:0.036, P_g:0.0096}), `corn_plot_1m2` (fertilization_per_m2: N_g=8, P_g=3, K_g=6), no manure producer; goals `tilapia_whole_kg=1.0/month` + `corn_grain_kg=1.0/month`; assert `composition_gap` diagnostic exists for `K_g_external`
- [X] T018 [P] [US2] Implement Scenario 5 test body in `tests/solver/test_composition_matching.cpp`: `tilapia_tank_5000l` + `lettuce_bed_1m2` (fertilization_per_m2: N_g=10, ...); goals `tilapia_whole_kg=1.0/month` + `lettuce_head=10.0/month`; assert a `ResourceFlow` edge exists in the result graph with `resource_slug=="nutrient_water_l"`

### Implementation for User Story 2

- [X] T019 [US2] Implement `run_fertilization_matching_pass()` as a free function in `src/solver/backpropagation.cpp`: iterate `EntityInstanceNode`s with non-empty `entity.fertilization_per_m2`; compute scaled demand per element (`per_m2[key] * area_m2 * qty * cpm`); collect candidates (graph nodes with N_g, P_g, or K_g in `Resource::composition`); sort by loop-closure contribution; allocate N_g-first from each candidate while crediting P_g and K_g simultaneously; unmet per-element remainder → `ExternalSourceNode "{key}_external"` + `DiagnosticKind::composition_gap`
- [X] T020 [US2] Extend `run_feed_matching_pass()` in `src/solver/backpropagation.cpp` to handle per-plant crop entities with N_g/P_g/K_g in `composition_requirements` (no `energy_kcal` key — e.g., banana_plant, papaya_plant, acerola_plant): implement the primary-key selection rule from contracts/public-api.md Pass 1 step 4 — when `energy_kcal` is absent but `N_g` is present, use N_g as the primary allocation driver and credit P_g/K_g as side benefits from the same allocation; scale demand by instance count (not area). Candidate collection, loop-closure sort, external node creation, and diagnostic emission are identical to the energy_kcal path.
- [X] T021 [US2] Update `ResourceBalance::composition_routed` in `run_fertilization_matching_pass()` for each allocated resource
- [X] T022 [US2] Wire `run_fertilization_matching_pass()` call into `BackpropagationSolver::solve()` immediately after the `run_feed_matching_pass()` call in `src/solver/backpropagation.cpp`

**Checkpoint**: `ctest --preset debug -R test_composition_matching` — all 5 scenarios pass.
All existing tests still pass.

---

## Phase 5: User Story 3 — Registry Migration (Priority: P3)

**Goal**: Default registry reflects composition-based declarations; slug-based feed and fertilizer
inputs are removed from all animal and crop entities; feed resources gain composition keys.

**Independent Test**: Load `data/default_registry.json` into a Registry; verify all animal and
crop entities have the expected `composition_requirements` or `fertilization_per_m2` maps and
zero `poultry_feed_kg`, `goat_feed_kg`, or `mature_compost_kg` inputs on migrated entities.

- [X] T023 [P] [US3] Migrate animal entities in `data/default_registry.json`: remove `poultry_feed_kg` from inputs of `broiler_chicken`, `laying_hen`, `quail`; add `composition_requirements` with values from research.md (broiler: protein_g=720, energy_kcal=11600; hen: 21.6/348; quail: 5.04/81.2)
- [X] T024 [P] [US3] Migrate goat entities in `data/default_registry.json`: remove `goat_feed_kg` from inputs of `goat` and `goat_kids`; add `composition_requirements` (goat: protein_g=180, energy_kcal=2700; kids: 60/900)
- [X] T025 [P] [US3] Migrate area-based crop entities in `data/default_registry.json`: remove `mature_compost_kg` input from `lettuce_bed_1m2`, `tomato_bed_1m2`, `pepper_bed_1m2`, `corn_plot_1m2`, `cassava_plot_1m2`; add `fertilization_per_m2` per data-model.md (lettuce: N=10/P=4/K=8; tomato: 15/6/12; pepper: 12/5/10; corn: 8/3/6; cassava: 5/2/8)
- [X] T026 [P] [US3] Migrate per-plant crop entities in `data/default_registry.json`: remove `mature_compost_kg` input from `banana_plant`, `papaya_plant`, `acerola_plant`; add `composition_requirements` with N_g/P_g/K_g per plant per cycle per data-model.md (banana: 120/40/200; papaya: 5/2/7; acerola: 3.3/1.5/4.0)
- [X] T027 [US3] Add `protein_g` and `energy_kcal` to the `composition` map of feed-capable resources in `data/default_registry.json`: `corn_grain_kg` (93.0/3580.0), `bean_kg` (220.0/3400.0), `cassava_kg` (14.0/1590.0), `cassava_leaves_kg` (200.0/900.0) — per data-model.md Feed resource composition table
- [X] T028 [US3] Update any existing tests in `tests/` that expected migrated slug inputs (e.g., `compost_bin` appearing in graph for corn goals, `poultry_feed_kg` appearing in animal solve plans) to reflect the new correct post-migration behavior — per SC-004 (no unintended regressions; migrations-caused test updates are expected and not counted as regressions)

**Checkpoint**: `ctest --preset debug` — all tests pass including the updated migrated-entity tests.
Registry validation accepts the migrated JSON without errors.

---

## Phase 6: User Story 4 — Regression Safety (Priority: P4)

**Goal**: Verify that a plan with no composition requirements produces output identical to the
pre-feature baseline. This story's primary test (Scenario 4) was already implemented in Phase 3
(T011). This phase performs the full regression sweep.

**Independent Test**: `ctest --preset debug` — every test that was passing on the `main` branch
continues to pass. No new failures introduced by any of the above phases.

- [X] T029 [US4] Run full test suite (`ctest --preset debug`) after all Phase 2–5 tasks complete; confirm zero test regressions unrelated to migrated entities; document any intentional test updates from T028 in a code comment referencing SC-004
- [X] T030 [US4] Verify Scenario 4 assertion: solve a plan with entities stripped of all `composition_requirements` and `fertilization_per_m2`; assert `loop_closure_score`, node count, and all `ResourceBalance` fields are bitwise-equal to the baseline result from the same goals using the pre-feature solver path

**Checkpoint**: Full test suite green. Scenario 4 passes. Loop closure score unchanged for
slug-only plans.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T031 [P] Run `clang-format` on all modified files (`include/homestead/core/entity.hpp`, `include/homestead/solver/result.hpp`, `include/homestead/serialization/schema_version.hpp`, `src/core/registry.cpp`, `src/solver/backpropagation.cpp`, `src/solver/analytics.cpp`, `src/serialization/serialization.cpp`, `tests/solver/test_composition_matching.cpp`, `tests/serialization/test_roundtrip.cpp`) and fix any formatting violations
- [X] T032 [P] Run `clang-tidy` on modified source files and resolve any warnings (in particular: unused variable in pass loops, range-for correctness, `unordered_map` value semantics)
- [X] T033 Build and run `ctest --preset sanitize` — verify no AddressSanitizer or UBSanitizer errors in the new composition passes or serialization paths
- [X] T034 Measure solver overhead for a 20-entity plan with composition matching enabled; confirm it is ≤ 50 ms added over baseline (SC-005) — add a comment in `backpropagation.cpp` near the pass calls with the measured value

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 2 (Foundational)**: No dependencies — start immediately
- **Phase 3 (US1)**: Depends on Phase 2 complete — `composition_requirements` field and `composition_gap` enum must exist
- **Phase 4 (US2)**: Depends on Phase 2 complete — can start in parallel with Phase 3 (different functions in backpropagation.cpp, but Phase 3's `run_feed_matching_pass()` wire-up in T015 must complete before T022 wires Pass 2 into the same `solve()` function)
- **Phase 5 (US3)**: Depends on Phase 2 complete — can start in parallel with Phase 3 and 4 (data/default_registry.json edits are independent from solver code)
- **Phase 6 (US4)**: Depends on Phase 3, 4, and 5 complete
- **Phase 7 (Polish)**: Depends on Phase 6 complete

### User Story Dependencies

- **US1 (P1)**: Start after Phase 2 — independent of US2, US3, US4 code changes
- **US2 (P2)**: Start after Phase 2 — depends on T015 (Pass 1 wired into solve()) before T022 (Pass 2 wired)
- **US3 (P3)**: Start after Phase 2 — fully independent (JSON data file edits only)
- **US4 (P4)**: Depends on US1, US2, US3 complete

### Within Each Phase

- T001 must complete before T005 (registry.cpp) and T006 (serialization.cpp)
- T002, T003 must complete before T010–T022
- T012 must complete before T015 (wire into solve())
- T019 must complete before T022 (wire into solve())
- T015 must complete before T022

### Parallel Opportunities Per Phase

**Phase 2:**
- T001 (entity.hpp) ‖ T002 + T003 (result.hpp — same file, sequential) ‖ T004 (schema_version.hpp)
- T005 + T006 + T007 (registry.cpp and serialization.cpp — different files, parallel) after T001 done
- T008 (test_roundtrip.cpp) and T009 (test_composition_matching.cpp) can run in parallel after T002, T003

**Phase 3:**
- T010 + T011 (different TEST_CASE blocks in same file — write sequentially to avoid conflict)
- T012 ‖ T014 (backpropagation.cpp function ‖ analytics.cpp) after tests written

**Phase 4:**
- T016 ‖ T017 ‖ T018 (different TEST_CASE stubs — write sequentially)
- T019 ‖ T020 ‖ T021 (different functions) after tests written

**Phase 5:**
- T023 ‖ T024 ‖ T025 ‖ T026 ‖ T027 (all edit `default_registry.json` — same file, do sequentially)

---

## Parallel Example: Phase 2

```
Sequential (same file):
  T002 → T003 (result.hpp additions)

Parallel across files:
  T001 (entity.hpp) ‖ T004 (schema_version.hpp)

After T001 done, parallel:
  T005 (registry.cpp) ‖ T006 + T007 (serialization.cpp)

After T002+T003 done, parallel:
  T008 (test_roundtrip.cpp) ‖ T009 (test_composition_matching.cpp scaffolding)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 2: Foundational (T001–T009)
2. Complete Phase 3: US1 — Pass 1 feed matching + loop closure update (T010–T015)
3. **STOP and VALIDATE**: `ctest --preset debug -R test_composition_matching` — Scenarios 1 and 4 pass
4. Ship MVP: broiler/poultry protein and energy routing from internally-produced grain works end-to-end

### Incremental Delivery

1. Phase 2 → Foundation ready (entity fields, serialization, enum value)
2. Phase 3 (US1) → Feed matching works → Validate (MVP!)
3. Phase 4 (US2) → Fertilizer routing works → Validate (all 5 scenarios pass)
4. Phase 5 (US3) → Registry migrated → Validate (full test suite green)
5. Phase 6 (US4) → Regression confirmed → Full test suite green
6. Phase 7 → Polish, sanitize, performance check → Ready to merge

---

## Notes

- **ASCII test names only**: All `TEST_CASE` names in `test_composition_matching.cpp` must use
  pure ASCII (no `—`, `²`, or other non-ASCII). CTest passes test names as filter strings on
  Windows; non-ASCII characters are mangled and cause "No test cases matched" failures.
- **Isolation guarantee**: Neither `run_feed_matching_pass()` nor `run_fertilization_matching_pass()`
  may call `add_entity_instance()`. This is a non-negotiable contract from public-api.md.
- **Feature 008 dependency**: Pass 2's fertilization candidates depend on `N_g`, `P_g`, `K_g`
  being present in `Resource::composition` for fertilizer resources. Until Feature 008 lands, only
  resources that already have these keys (or those added in T027 for feed resources) will be matched.
  Scenarios 2, 3, and 5 may require Feature 008's composition keys to pass fully.
- **[P] tasks in same file**: When two tasks are marked [P] but edit the same file, perform them
  sequentially within that file to avoid conflicts.
- Commit after each phase checkpoint — not after every individual task.
