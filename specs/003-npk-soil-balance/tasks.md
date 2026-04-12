# Tasks: Nutrient and Soil Balance Tracking (N-P-K)

**Branch**: `003-npk-soil-balance`  
**Input**: Design documents from `specs/003-npk-soil-balance/`  
**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Data Model**: [data-model.md](data-model.md)

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Parallelizable (different files, no dependency on incomplete tasks)
- **[Story]**: User story label — US1, US2, US3
- Tests are **required** per the spec (Constitution §III + FR-011, SC-001–SC-006)

---

## Phase 1: Setup (New Files & Scaffolding)

**Purpose**: Create all new files and hook them into the build system. No logic yet.

- [x] T001 [P] Create `include/homestead/core/nutrient.hpp` with `NutrientDemand` struct (three `double` fields, all defaulted to `0.0`, full Doxygen `///` comments)
- [x] T002 [P] Add `DiagnosticKind::nutrient_deficit` enumerator to `include/homestead/solver/result.hpp` with Doxygen comment
- [x] T003 Add `NutrientBalance` struct (six `MonthlyValues` fields) to `include/homestead/solver/result.hpp` with Doxygen comments
- [x] T004 Add `std::optional<NutrientBalance> nutrient_balance` field to `PlanResult` in `include/homestead/solver/result.hpp` with Doxygen comment
- [x] T005 [P] Add `#include <homestead/core/nutrient.hpp>` and `std::optional<NutrientDemand> nutrient_demand` field to `Entity` in `include/homestead/core/entity.hpp` with Doxygen comment
- [x] T006 [P] Bump `PLAN_SCHEMA_VERSION` from `{1, 0, 0}` to `{1, 1, 0}` in `include/homestead/serialization/schema_version.hpp`
- [x] T007 Create `tests/solver/test_nutrient_balance.cpp` with Catch2 boilerplate (`#include`, `TEST_CASE` stubs for 6 tests) and add it to `tests/solver/CMakeLists.txt`

**Note**: T001, T002, T005, T006, T007 touch different files — all are [P]. T003 and T004 both edit `result.hpp` — run T003 before T004.

**Checkpoint**: Project compiles with new empty types (no logic yet). Run `cmake --preset debug && cmake --build --preset debug`.

---

## Phase 2: Foundational (Core Logic — Blocks All User Stories)

**Purpose**: Implement the analytics engine and serialization. These components are
prerequisites for all three user stories.

**⚠️ CRITICAL**: No user story tests can pass until this phase is complete.

- [x] T008 Implement `extract_npk()` internal helper (unit detection by `_ppm`/`_percent` suffix, grams conversion formulas) in `src/solver/analytics.cpp`
- [x] T009 Implement `compute_nutrient_balance()` in `src/solver/analytics.cpp`:
  - Supply side: gross production from `balance_sheet` minus feedstock consumed by non-crop entities (graph iteration skipping entities with `nutrient_demand` set)
  - Demand side: graph iteration over entities with `nutrient_demand`, using `cycles_in_month()` for prorated monthly allocation
  - Return `std::nullopt` when no crop entity instances present
- [x] T010 Add `compute_nutrient_balance()` call in `populate_plan_result()` in `src/solver/analytics.cpp` (after `check_labor_constraint`, before diagnostics check)
- [x] T011 [P] Implement `check_nutrient_deficits()` in `src/solver/convergence.cpp`: iterate all three elements, emit one `DiagnosticKind::nutrient_deficit` per element-month where `demanded > available`, message format: `"{Element} deficit in {Mon}: {avail:.1f} g available vs {demand:.1f} g demanded (shortfall: {shortfall:.1f} g)"`
- [x] T012 Add `check_nutrient_deficits()` call in `populate_plan_result()` in `src/solver/analytics.cpp` (after T010's call)
- [x] T013 Serialize `Entity::nutrient_demand` in `entity_to_json()` (omit field when `nullopt`) in `src/serialization/serialization.cpp`
- [x] T014 Deserialize `Entity::nutrient_demand` in `entity_from_json()` (set `std::nullopt` when field absent) in `src/serialization/serialization.cpp`
- [x] T015 Serialize `PlanResult::nutrient_balance` in plan result serializer (omit field when `nullopt`) in `src/serialization/serialization.cpp`
- [x] T016 Deserialize `PlanResult::nutrient_balance` in plan result deserializer with Q2 rules: field present → parse; field absent AND version < 1.1.0 → `NutrientBalance{}`; field absent AND version ≥ 1.1.0 → `std::nullopt` in `src/serialization/serialization.cpp`
- [x] T017 [P] Add `nutrient_deficit` to the `DiagnosticKind` serializer `to_string()` switch and the reverse-lookup deserializer in `src/serialization/serialization.cpp`
- [x] T017b Run `ctest --preset debug` (full suite, all pre-existing tests) and confirm zero regressions before starting any user story phase (SC-004)

**Checkpoint**: `cmake --build --preset debug` succeeds. All existing tests still pass: `ctest --preset debug`.

---

## Phase 3: User Story 1 — Deficit Detection (Priority: P1) 🎯 MVP

**Goal**: The solver emits precise `nutrient_deficit` diagnostics whenever monthly
supply falls short of crop demand, with exact element, month, and shortfall values.

**Independent Test**: Build a plan with a large corn plot and a small chicken coop.
Run the solver. Assertions: `nutrient_balance.has_value()` is true; at least one
`DiagnosticKind::nutrient_deficit` is present for nitrogen; diagnostic message
contains "Nitrogen", the correct month, and a shortfall > 0.

- [x] T018 [P] [US1] Write Test 4 (unit conversion) in `tests/solver/test_nutrient_balance.cpp`: manually construct solid resource with `N_percent=1.5`, produce 100 kg net → assert `available_n` = 1500 g ± 1e-9; construct liquid resource with `N_ppm=30.0`, produce 1000 L net → assert `available_n` = 30 g ± 1e-9
- [x] T019 [P] [US1] Write Test 3 (no-crop nullopt) in `tests/solver/test_nutrient_balance.cpp`: plan with broiler chickens + tilapia only; assert `!result.nutrient_balance.has_value()`
- [x] T019b [P] [US1] Write isolation invariant test in `tests/solver/test_nutrient_balance.cpp`: run the same graph twice — once with `nutrient_demand` populated on a crop entity, once with it absent — assert `balance_sheet` quantities and graph node counts are identical in both runs (FR-008)
- [x] T020 [US1] Write Test 2 (nitrogen deficit) in `tests/solver/test_nutrient_balance.cpp`: large corn plot (10 m²) + minimal chicken coop; hand-compute expected `demanded_n` per month (8.0 g/m²/cycle × 10 m² × cycles_in_month) and expected `available_n`; assert at least one `nutrient_deficit` diagnostic emitted for nitrogen with correct shortfall (pre-calculated from registry values)
- [x] T021 [US1] Run `ctest --preset debug -R test_nutrient_balance` and confirm T018–T020 pass; run `ctest --preset sanitize -R test_nutrient_balance` with zero ASan/UBSan errors

**Checkpoint**: Deficit detection works end-to-end. User Story 1 independently verifiable.

---

## Phase 4: User Story 2 — Monthly N-P-K Breakdown (Priority: P2)

**Goal**: `NutrientBalance` round-trips through JSON without data loss; monthly arrays
correctly zero out inactive months for seasonal crops.

**Independent Test**: Serialize a `PlanResult` with non-trivial `NutrientBalance` to JSON
string, deserialize, compare all six 12-element arrays element-by-element within 1e-9.
Separately verify that a plan with a crop active in months 4–6 only has zero
`demanded_n` for months 1–3 and 7–12.

- [x] T022 [P] [US2] Write Test 5 (JSON round-trip) in `tests/solver/test_nutrient_balance.cpp`: produce `PlanResult` with known non-zero `nutrient_balance`, serialize via `homestead::serialization`, deserialize, assert all six arrays match element-by-element ± 1e-9
- [x] T023 [P] [US2] Write Test 6 (schema 1.0.x backward compat) in `tests/solver/test_nutrient_balance.cpp`: deserialize a hard-coded JSON string with `"version": "1.0.0"` and no `nutrient_balance` field; assert `result.nutrient_balance.has_value()` is **true** and all twelve values in `available_n` equal 0.0 (Q2: zero-init, not nullopt)
- [x] T024 [P] [US2] Add round-trip assertions for `Entity::nutrient_demand` in `tests/serialization/test_roundtrip.cpp`: entity with demand set → JSON → entity with identical demand; entity with `nullopt` demand → JSON → no `nutrient_demand` key present
- [x] T025 [US2] Add round-trip assertions for `PlanResult::nutrient_balance` in `tests/serialization/test_roundtrip.cpp`: plan with non-trivial `nutrient_balance` → serialize → deserialize → all six arrays identical
- [x] T026 [US2] Run `ctest --preset debug -R "test_nutrient|test_roundtrip"` and confirm all new assertions pass; run `ctest --preset sanitize` for full suite

**Checkpoint**: Serialization contract verified. User Story 2 independently verifiable.

---

## Phase 5: User Story 3 — Registry Nutrient Data (Priority: P3)

**Goal**: The default registry ships with authoritative Embrapa N-P-K values for all
organic amendment resources and `nutrient_demand` for all area-based crop entities.

**Independent Test**: Load the default registry. For each resource with a slug containing
"manure", "compost", or "humus", assert all composition values are strictly positive.
For each of the six crop entities, assert `nutrient_demand` is present with
`n_g_per_m2_per_cycle > 0`.

- [x] T026b [US3] Audit `data/default_registry.json`: for every resource whose slug contains "manure", "compost", "humus", "digestate", or "fertilizer", verify `N_percent`/`P_percent`/`K_percent` (or `N_ppm`/`P_ppm`) are all populated; add any missing values from Embrapa technical bulletins with a `"_source"` comment (FR-009)
- [x] T027 [US3] Add `nutrient_demand` to six area-based crop entities in `data/default_registry.json`: `lettuce_bed_1m2` (N=10, P=4, K=8), `tomato_bed_1m2` (N=15, P=6, K=12), `pepper_bed_1m2` (N=12, P=5, K=10), `corn_plot_1m2` (N=8, P=3, K=6), `bean_plot_1m2` (N=4, P=3, K=5), `cassava_plot_1m2` (N=5, P=2, K=8). Add `"_source"` metadata string per entity. Add TODO comment near `banana_plant`/`papaya_plant`/`acerola_plant` noting per-plant demand as future work.
- [x] T028 [US3] Write Test 1 (Sisteminha integration) in `tests/solver/test_nutrient_balance.cpp`: bean_plot_1m2 + tilapia_tank_5000l plan — pre-calculated bean qty=12, tilapia qty=1; available_n=36g, available_p=9.6g, available_k=0g; demanded_n=16g (surplus), demanded_p=12g (deficit), demanded_k=20g (deficit) → 12× P deficit + 12× K deficit, 0× N deficit
- [x] T029 [US3] Run `ctest --preset debug` (full suite) and confirm all tests pass including T028; run `ctest --preset sanitize` with zero errors

**Checkpoint**: Default registry complete. All three user stories independently verifiable with real Sisteminha data.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [x] T030 [P] Run `clang-format` on all modified and new files (`include/homestead/core/nutrient.hpp`, `include/homestead/core/entity.hpp`, `include/homestead/solver/result.hpp`, `src/solver/analytics.cpp`, `src/solver/convergence.cpp`, `src/serialization/serialization.cpp`, `tests/solver/test_nutrient_balance.cpp`, `tests/serialization/test_roundtrip.cpp`) and fix any formatting violations
- [x] T031 [P] Run `clang-tidy` on modified files and fix all warnings (2 pre-existing false-positive warnings remain: `parse` static method, `ElementView` const-ref members)
- [x] T032 Run release build (optimized) to verify no release-mode regressions (release preset has no test target; library builds clean)
- [x] T033 Verify `quickstart.md` examples compile and produce expected output using the CLI harness (`homestead::cli`)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 complete — **blocks all user stories**
- **Phase 3 (US1)**: Depends on Phase 2
- **Phase 4 (US2)**: Depends on Phase 2 (can run in parallel with Phase 3)
- **Phase 5 (US3)**: Depends on Phase 2 (can run in parallel with Phases 3 and 4)
- **Phase 6 (Polish)**: Depends on all desired user story phases complete

### User Story Dependencies

- **US1 (P1)**: No dependency on US2 or US3 — independently verifiable after Phase 2
- **US2 (P2)**: No dependency on US1 or US3 — tests serialization contract independently
- **US3 (P3)**: No dependency on US1 or US2 — tests registry data independently; T028 integration test does exercise the full stack but is written against the Phase 2 analytics engine, not US1/US2 test outcomes

### Within Phase 2 (critical path)

```
T008 (extract_npk helper)
  └── T009 (compute_nutrient_balance)
        └── T010 (wire into populate_plan_result)
              └── T012 (wire check_nutrient_deficits)
T011 (check_nutrient_deficits) — [P] with T008–T010
T013 → T014 (entity serialization, sequential — same function pair)
T015 → T016 (plan result serialization, sequential — same function pair)
T017 [P] (DiagnosticKind serializer — different switch block)
```

### Parallel Opportunities

- **Phase 1**: T001, T002, T005, T006, T007 all touch different files — launch together
- **Phase 2**: T008–T012 form one chain; T011 [P] with that chain; T013–T017 form a second chain starting after Phase 1
- **Phase 3**: T018 and T019 are [P] (different test cases in same file — write as stubs first, then fill); T020 depends on T018/T019 to understand the test fixture
- **Phase 4**: T022, T023, T024 are [P]; T025 depends on T022
- **Phase 5**: T027 before T028 (registry data must be present for integration test)

---

## Parallel Example: Phase 1

```
Parallel batch 1 — launch together:
  T001: include/homestead/core/nutrient.hpp           (new file)
  T002: include/homestead/solver/result.hpp            (add enum value)
  T005: include/homestead/core/entity.hpp              (add field)
  T006: include/homestead/serialization/schema_version.hpp  (bump constant)
  T007: tests/solver/test_nutrient_balance.cpp + CMakeLists.txt  (scaffold)

Sequential after T002:
  T003: result.hpp — add NutrientBalance struct
  T004: result.hpp — add PlanResult::nutrient_balance field
```

## Parallel Example: Phase 3 (US1)

```
Parallel batch — write test stubs first:
  T018: Test 4 (unit conversion)
  T019: Test 3 (no-crop nullopt)

Sequential after T018 + T019 confirm the fixture setup pattern:
  T020: Test 2 (nitrogen deficit, uses full plan fixture)
  T021: ctest run
```

---

## Implementation Strategy

### MVP (User Story 1 only)

1. Complete Phase 1 — new types compile
2. Complete Phase 2 — analytics engine wired
3. Complete Phase 3 — deficit detection tested
4. **STOP and VALIDATE**: `ctest --preset debug -R test_nutrient_balance` — US1 independently proven

### Incremental Delivery

1. Phase 1 + Phase 2 → engine ready
2. Phase 3 (US1) → deficit detection works → MVP
3. Phase 4 (US2) → serialization contract locked → persist and reload plans
4. Phase 5 (US3) → real Embrapa data ships in default registry → integration validated
5. Phase 6 → polish, clang-format/tidy, release build clean

---

## Notes

- All six `[P]` marks in Phase 1 are safe because they touch separate files
- `result.hpp` is edited in three sequential steps (T002 → T003 → T004) to keep each diff reviewable
- Tests **must** hard-code pre-calculated expected values (Q5 clarification) — do not use dynamic tolerances that accept both deficit and surplus
- The backward-compat test (T023) explicitly tests the `NutrientBalance{}` zero-init path for 1.0.x files (Q2) — do not skip
- Registry `_source` metadata strings are ignored by the deserializer; they are documentation only and must not cause parse errors
- `banana_plant`, `papaya_plant`, `acerola_plant` intentionally excluded from T027 — add TODO comment, not implementation
