---
description: "Task list for homestead-engine core library implementation"
---

# Tasks: homestead-engine Core Library

**Input**: Design documents from `specs/001-homestead-engine-core/`
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/public_api.md ✅

**Tests**: Included — every public API surface requires unit tests per constitution Principle III.

**Organization**: Tasks are grouped by user story to enable independent implementation
and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1–US5)
- Paths are relative to repository root

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, build system, tooling, CI skeleton.

- [x] T001 Create top-level `CMakeLists.txt` with `cmake_minimum_required(VERSION 3.28)`, project declaration, `HOMESTEAD_BUILD_TESTS`, `HOMESTEAD_BUILD_CLI`, `HOMESTEAD_ENABLE_LOGGING` options, and `add_subdirectory` calls for `src/core`, `src/graph`, `src/solver`, `src/serialization`
- [x] T002 Create `CMakePresets.json` with `debug` (`-O0 -g`, tests ON), `release` (`-O3 -DNDEBUG`, tests OFF), and `sanitize` (ASan+UBSan, tests ON) presets per plan.md
- [x] T003 [P] Create `.clang-format` at repo root using Google style base with `ColumnLimit: 100`, `PointerAlignment: Left`, `Standard: c++23`
- [x] T004 [P] Create `.clang-tidy` at repo root enabling `modernize-*`, `cppcoreguidelines-*`, `performance-*`, `readability-*` checks; disable `modernize-use-trailing-return-type`
- [x] T005 [P] Create directory skeleton: `include/homestead/core/`, `include/homestead/graph/`, `include/homestead/solver/`, `include/homestead/serialization/`, `src/core/`, `src/graph/`, `src/solver/`, `src/serialization/`, `tests/core/`, `tests/graph/`, `tests/solver/`, `tests/serialization/`, `data/`
- [x] T006 [P] Create `src/core/CMakeLists.txt`: define `homestead_core` target, `add_library(homestead_core STATIC)`, public headers `include/homestead/core/`, ZERO non-stdlib dependencies; alias as `homestead::core`
- [x] T007 [P] Create `src/graph/CMakeLists.txt`: define `homestead_graph` STATIC target, PUBLIC link `homestead::core`; alias as `homestead::graph`
- [x] T008 [P] Create `src/solver/CMakeLists.txt`: define `homestead_solver` STATIC target, PUBLIC link `homestead::core homestead::graph`; alias as `homestead::solver`
- [x] T009 [P] Create `src/serialization/CMakeLists.txt`: define `homestead_serialization` STATIC target, PUBLIC link `homestead::core`; PRIVATE link `nlohmann_json::nlohmann_json`; alias as `homestead::serialization`
- [x] T010 Create `FetchContent` block in top-level CMakeLists.txt for nlohmann/json v3.11.3, Catch2 v3.6.0, and spdlog v1.13.0 (spdlog wrapped in `if(HOMESTEAD_ENABLE_LOGGING)`)
- [x] T011 [P] Create `tests/core/CMakeLists.txt`, `tests/graph/CMakeLists.txt`, `tests/solver/CMakeLists.txt`, `tests/serialization/CMakeLists.txt` — each creates a Catch2-based executable and registers with CTest via `catch_discover_tests`
- [x] T012 [P] Create `.github/workflows/ci.yml` with matrix strategy: GCC 13 (ubuntu-latest), Clang 17 (ubuntu-latest), MSVC latest (windows-latest); each job runs `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`; add a separate sanitize job using `--preset sanitize` on ubuntu with GCC 13
- [x] T013 [P] Create `src/core/log.hpp` (private, never installed) with `HOMESTEAD_LOG_DEBUG`, `HOMESTEAD_LOG_INFO`, `HOMESTEAD_LOG_WARN` macros that compile to no-ops when `HOMESTEAD_ENABLE_LOGGING` is OFF and delegate to `spdlog` when ON; this file MUST NOT be placed under `include/homestead/` — constitution Principle II forbids non-stdlib includes in core's public headers (per research.md §8)
- [x] T014 [P] Create `data/default_registry.json` skeleton with `"version": "1.0.0"`, `"type": "registry"`, and empty `"resources": []` / `"entities": []` arrays; add `configure_file` in CMakeLists.txt to embed path at compile time

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure types that ALL user stories depend on.

⚠️ **CRITICAL**: No user story work can begin until this phase is complete.

- [x] T015 [P] Implement `VariableQuantity` in `include/homestead/core/quantity.hpp`: three `double` fields (`min`, `expected`, `max`), `VariableQuantity(double fixed)` constructor, `VariableQuantity(double, double, double)` constructor with `std::expected<VariableQuantity, std::string>` factory that validates `0 ≤ min ≤ expected ≤ max`; add `MonthMask` type alias, `ALL_MONTHS`, `NO_MONTHS` constants, and `is_active(MonthMask, int)` helper per data-model.md
- [x] T016 [P] Implement `ResourceCategory` enum class and `ChemicalComposition` / `NutritionalProfile` / `PhysicalProperties` types in `include/homestead/core/resource.hpp` per data-model.md; all value types, no raw pointers
- [x] T017 Implement `Resource` struct in `include/homestead/core/resource.hpp`: all fields from data-model.md; add slug validation helper `is_valid_slug(std::string_view)` (regex `[a-z0-9_]+`, non-empty); add Doxygen `///` comments on all public members
- [x] T018 [P] Implement `Lifecycle`, `ResourceFlowSpec`, `InfrastructureSpec` types in `include/homestead/core/entity.hpp` per data-model.md; value semantics; Doxygen comments
- [x] T019 Implement `Entity` struct in `include/homestead/core/entity.hpp` with all fields from data-model.md; at least one output required; Doxygen comments
- [x] T020 [P] Implement `RegistryError` (`RegistryErrorKind` enum + struct with `kind`, `message`, `offending_slug`) in `include/homestead/core/registry.hpp`
- [x] T021 Implement `Registry` class in `include/homestead/core/registry.hpp` + `src/core/registry.cpp`: `load_defaults()`, `register_resource()`, `register_entity()` with eager slug validation, `find_resource()`, `find_entity()`, `resources()`, `entities()`, `producers_of()` per data-model.md contracts; override semantics (re-registration replaces); Doxygen on all public methods
- [x] T022 Write unit tests for `VariableQuantity` and `MonthMask` in `tests/core/test_quantity.cpp`: construction, invariant validation, fixed-value shorthand, `is_active` for all 12 months
- [x] T023 [P] Write unit tests for `Resource` in `tests/core/test_resource.cpp`: valid construction, slug validation (rejects empty, rejects invalid chars), optional NutritionalProfile present/absent
- [x] T024 [P] Write unit tests for `Entity` in `tests/core/test_entity.cpp`: valid entity, entity with zero outputs rejected, entity with invalid lifecycle rejected
- [x] T025 Write unit tests for `Registry` in `tests/core/test_registry.cpp`: load_defaults succeeds, register resource, register entity with valid slugs, register entity with unknown resource slug returns `RegistryError::unknown_resource_slug`, override replaces existing, `producers_of` returns correct entity slugs

**Checkpoint**: `cmake --preset debug && ctest --preset debug -R test_quantity -R test_resource -R test_entity -R test_registry` all pass → Foundational phase complete, user story phases can begin.

---

## Phase 3: User Story 1 — Define and Query Resources and Entities (Priority: P1) 🎯 MVP

**Goal**: Developer can load the default registry, query resources/entities, and register
custom ones with eager validation.

**Independent Test**: Load default registry → query `"chicken_manure_kg"` → define
custom entity → register → query back. No graph or solver required.

### Implementation for User Story 1

- [x] T026 [US1] Populate `data/default_registry.json` with the 20 entity templates and ≥60 resource slugs from research.md §9 (Sisteminha modules: broiler_chicken, laying_hen, quail, tilapia_tank_5000l, compost_bin, earthworm_bin, biodigester, lettuce_bed_1m2, tomato_bed_1m2, pepper_bed_1m2, corn_plot_1m2, bean_plot_1m2, cassava_plot_1m2, banana_plant, papaya_plant, acerola_plant, goat, goat_kids, water_tank, rainwater_collector); values from Embrapa tropical agriculture references; every field is a `(min, expected, max)` triple
- [x] T027 [US1] Implement `Registry::load_defaults()` to deserialize `default_registry.json` at startup (embed via CMake `configure_file` or compile-time string literal in `src/core/registry.cpp`); must succeed without any file I/O at runtime
- [x] T028 [US1] Write integration test in `tests/core/test_registry.cpp` asserting that `Registry::load_defaults()` returns a registry containing all 20 expected entity slugs and that `find_resource("chicken_manure_kg")` returns a resource with non-empty `composition`
- [x] T029 [US1] Validate the quickstart.md custom-entity example compiles and runs correctly: add `tests/core/test_registry.cpp` scenario that registers `"black_soldier_fly_larvae_kg"` resource then `"bsf_bin"` entity and verifies round-trip retrieval

**Checkpoint**: `ctest --preset debug -R test_registry` passes including the default-registry and custom-entity scenarios → User Story 1 independently functional.

---

## Phase 4: User Story 2 — Build and Inspect a Resource-Flow Graph (Priority: P2)

**Goal**: Developer can construct a directed resource-flow graph with entity instances,
external sources, and goal sinks; query connectivity; detect cycles.

**Independent Test**: Construct three-node graph (source → entity instance → goal sink),
verify directed edges, query neighbors, introduce A→B→A cycle, confirm `has_cycle()`
returns true and `find_cycles()` enumerates it.

### Implementation for User Story 2

- [x] T030 [P] [US2] Implement `NodeId`, `NodeKind`, `EntityInstanceNode`, `ExternalSourceNode`, `GoalSinkNode` in `include/homestead/graph/node.hpp` per data-model.md; value types; Doxygen comments
- [x] T031 [P] [US2] Implement `ResourceFlow` edge type in `include/homestead/graph/edge.hpp` per data-model.md; value type; Doxygen comments
- [x] T032 [P] [US2] Implement `GraphError` (`GraphErrorKind` enum + struct) in `include/homestead/graph/graph.hpp`
- [x] T033 [US2] Implement `Graph` class in `include/homestead/graph/graph.hpp` + `src/graph/graph.cpp`: adjacency-list using `std::unordered_map<NodeId, std::vector<NodeId>>` for successors/predecessors; `std::unordered_map<NodeId, NodeVariant>` for node data; `add_entity_instance`, `add_external_source`, `add_goal_sink`, `add_flow`, `remove_node`; `successors`, `predecessors`, `get_node`, `node_count`, `edge_count` per data-model.md and contracts/public_api.md; Doxygen on all public methods
- [x] T034 [US2] Implement `Graph::has_cycle()` using iterative DFS with white/grey/black colouring in `src/graph/graph.cpp`
- [x] T035 [US2] Implement `Graph::find_cycles()` using Johnson's algorithm for simple cycle enumeration in `src/graph/graph.cpp` (per research.md §4); fallback to DFS for existence-only
- [x] T036 [US2] Implement `Graph::unsatisfied_inputs(const Registry&)` in `src/graph/graph.cpp`: for each `EntityInstanceNode`, check all entity inputs against incoming `ResourceFlow` edges; return `(NodeId, resource_slug)` pairs with no incoming flow
- [x] T037 [P] [US2] Write unit tests for graph construction in `tests/graph/test_graph_construction.cpp`: add nodes, add flows, remove node removes edges, `node_count`, `edge_count`, `successors`, `predecessors`, `get_node`; verify `GraphError::self_loop` on self-edge
- [x] T038 [P] [US2] Write unit tests for cycle detection in `tests/graph/test_cycle_detection.cpp`: acyclic graph returns false/empty, two-node cycle detected, chicken→manure→compost→corn→feed→chicken cycle enumerated correctly by `find_cycles()`
- [x] T039 [P] [US2] Write unit tests for traversal in `tests/graph/test_traversal.cpp`: `unsatisfied_inputs` returns correct gaps, satisfied inputs not reported, external source node has no unsatisfied inputs

**Checkpoint**: `ctest --preset debug -R test_graph` all pass → User Story 2 independently functional.

---

## Phase 5: User Story 3 — Solve Backwards from Production Goals (Priority: P3)

**Goal**: Developer specifies production goals; solver returns a complete `PlanResult`
with all entity instances, scheduling, and `Diagnostics` list.

**Independent Test**: Goal of "20 kg tilapia per month" → solver returns `PlanResult`
containing a tilapia tank entity instance, correct cycle count, and all upstream inputs
(fingerlings, feed, water, electricity) listed as internal or external purchase.

### Implementation for User Story 3

- [x] T040 [P] [US3] Implement `SolverConfig`, `Scenario` enum, and `ProductionGoal` in `include/homestead/solver/config.hpp` per data-model.md; default values as specified; Doxygen comments
- [x] T041 [P] [US3] Implement `MonthlyValues`, `ResourceBalance`, `DiagnosticKind`, `Diagnostic`, `InfrastructureBOM`, and `PlanResult` in `include/homestead/solver/result.hpp` per data-model.md; value types; Doxygen on all members
- [x] T042 [P] [US3] Implement `ISolverStrategy` pure interface in `include/homestead/solver/strategy.hpp` per contracts/public_api.md; virtual destructor; `solve()` pure virtual method
- [x] T043 [US3] Implement the backpropagation algorithm in `src/solver/backpropagation.cpp`: demand-driven backward expansion, entity selection by loop-closure contribution (FR-011), external purchase fallback (FR-007); never throws; per research.md §1
- [x] T044 [US3] Implement fixed-point convergence loop in `src/solver/convergence.cpp`: Gauss-Seidel style, tolerance 1e-6, max iterations from `SolverConfig::max_convergence_iterations` (default 100); record `DiagnosticKind::non_convergent_cycle` on cap exceeded; per research.md §2
- [x] T045 [US3] Implement monthly time-step scheduling in `src/solver/scheduling.cpp`: cycle-to-month mapping using `Lifecycle::active_months` (`MonthMask`); distribute cycles_per_year across active months; flag `DiagnosticKind::seasonality_gap` when goal month has no active entity; per research.md §5
- [x] T046 [US3] Implement `BackpropagationSolver` class in `include/homestead/solver/solver.hpp` + `src/solver/backpropagation.cpp` implementing `ISolverStrategy`; implement `homestead::solve()` free function convenience API per contracts/public_api.md
- [x] T047 [US3] Write integration test in `tests/solver/test_backpropagation.cpp`: single-goal scenario "20 kg tilapia_whole_kg per month" against default registry; assert PlanResult contains tilapia_tank_5000l instance, cycle count ≥ 1, upstream inputs present in balance sheet; hand-calculated expected values
- [x] T048 [P] [US3] Write integration test in `tests/solver/test_convergence.cpp`: canonical circular dependency (chicken_manure → compost_bin → mature_compost → corn_plot → corn_grain → broiler_chicken feed → chicken_manure); assert solver converges, diagnostics is empty, internal production ≥ consumption for closed resources (SC-004)
- [x] T049 [P] [US3] Write integration test in `tests/solver/test_scheduling.cpp`: goal requiring lettuce (seasonal) in a month outside its `active_months`; assert `DiagnosticKind::seasonality_gap` present in diagnostics
- [x] T050 [P] [US3] Write integration test in `tests/solver/test_constraints.cpp`: area constraint smaller than needed; assert `DiagnosticKind::area_exceeded` in diagnostics, solver does not over-allocate, continues satisfying goals that fit
- [x] T070 [US3] Implement budget constraint enforcement in `src/solver/backpropagation.cpp`: when `SolverConfig::max_budget` is set and the plan's estimated initial cost would exceed it, record `DiagnosticKind::unsatisfied_goal` for the over-budget goals and continue solving remaining goals; update `SolverConfig` usage in `src/solver/constraints.cpp` (or inline in backpropagation.cpp)
- [x] T071 [US3] Implement labor-hours-per-month constraint enforcement in `src/solver/scheduling.cpp`: when `SolverConfig::max_labor_hours_per_month` is set and a month's total labor exceeds the limit, flag affected entity instances in Diagnostics as `DiagnosticKind::unsatisfied_goal` with a descriptive message identifying the constrained month
- [x] T072 [P] [US3] Write integration tests in `tests/solver/test_constraints.cpp` for budget and labor constraints: (a) budget constraint exceeded → `DiagnosticKind::unsatisfied_goal` present, total initial cost of accepted entities ≤ max_budget; (b) labor constraint exceeded in one month → diagnostic present identifying that month, labor for other months unaffected
- [x] T051 [US3] Write property-based tests in `tests/solver/test_properties.cpp` using Catch2 `GENERATE`: (1) balance invariant — `internal_production + external_purchase ≥ consumption` for all resources all months when diagnostics empty; (2) loop score in [0.0, 1.0]; (3) idempotence — same inputs produce identical PlanResult; (4) solver never throws for any input including empty goals

**Checkpoint**: `ctest --preset debug -R test_backpropagation -R test_convergence -R test_scheduling -R test_constraints -R test_properties` all pass → User Story 3 independently functional.

---

## Phase 6: User Story 4 — Analyze a Solved Plan (Priority: P4)

**Goal**: Developer extracts resource balance sheet (monthly), gap report, labor schedule,
infrastructure BOM, and loop closure score from a solved plan.

**Independent Test**: Solve known two-entity plan (broiler_chicken + corn_plot); assert
balance sheet has monthly rows for all resources; assert loop closure score within
hand-calculated expected range ±0.01; assert labor schedule non-zero for active months.

### Implementation for User Story 4

- [x] T052 [US4] Implement analytics functions in `src/solver/analytics.cpp`: `compute_balance_sheet()` producing `ResourceBalance` with 12-month arrays + annual totals; `compute_gap_report()` filtering to external-purchase resources; `compute_labor_schedule()` summing entity instance labor across months; `compute_bom()` summing area and construction materials; `compute_loop_closure_score()` using economic-value-weighted formula from research.md §3; integrate into `BackpropagationSolver::solve()` output
- [x] T053 [US4] Write unit tests for analytics in `tests/solver/test_analytics.cpp`: fully-closed plan → loop score 1.0; fully-external plan → loop score 0.0; verify monthly sum equals annual total for balance sheet; verify BOM area matches sum of entity instance areas; verify labor schedule month [0] matches January entity operating hours (hand-calculated)
- [x] T054 [US4] Add `to_string(DiagnosticKind)` utility function in `include/homestead/solver/result.hpp` for use in the quickstart.md example output

**Checkpoint**: `ctest --preset debug -R test_analytics` passes → User Story 4 independently functional.

---

## Phase 7: User Story 5 — Serialize and Deserialize Plans and Registries (Priority: P5)

**Goal**: Developer can save Registry/Graph/PlanResult to JSON and reload without data loss
or re-solving; schema version mismatch detected and reported.

**Independent Test**: Serialize registry with 3 custom entities → deserialize → assert
all entries present including `(min, expected, max)` triples.

### Implementation for User Story 5

- [x] T055 [P] [US5] Implement `SchemaVersion` struct in `include/homestead/serialization/schema_version.hpp`: `major`/`minor`/`patch` fields, `to_string()`, `parse()` returning `std::expected<SchemaVersion, std::string>`, `compatible_with()` checking MAJOR only; per data-model.md
- [x] T056 [US5] Implement `to_json(const Registry&)` and `registry_from_json(const nlohmann::json&)` in `src/serialization/serialization.cpp`: JSON envelope with `"version": "1.0.0"`, `"type": "registry"`, `"data"` object; full round-trip for all Resource and Entity fields including VariableQuantity triples; return `std::expected<Registry, std::string>` on deserialization
- [x] T057 [US5] Implement `to_json(const Graph&)` and `graph_from_json(const nlohmann::json&)` in `src/serialization/serialization.cpp` per contracts/public_api.md JSON envelope spec
- [x] T058 [US5] Implement `to_json(const PlanResult&)` and `plan_from_json(const nlohmann::json&)` in `src/serialization/serialization.cpp`: must preserve full `balance_sheet` (monthly arrays), `labor_schedule`, `bom`, `diagnostics`, and `loop_closure_score` so plan can be reconstructed without re-solving
- [x] T059 [US5] Add schema version check in all `from_json` functions: return descriptive `std::string` error when `"version"` field is missing, when MAJOR differs from supported MAJOR (1), or when required fields are missing/wrong-type; error must identify the offending field
- [x] T060 [P] [US5] Write unit tests in `tests/serialization/test_roundtrip.cpp`: Registry full round-trip (all 20 default entities + 3 custom); Graph round-trip preserving all node types and edges; PlanResult round-trip (balance sheet values identical after deserialize, no re-solve required)
- [x] T061 [P] [US5] Write unit tests in `tests/serialization/test_schema_version.cpp`: MAJOR mismatch returns error with version info; missing `"version"` field returns error; missing required inner field returns error naming the field; malformed `VariableQuantity` (non-array) returns error; valid current-version document deserializes cleanly

**Checkpoint**: `ctest --preset debug -R test_roundtrip -R test_schema_version` all pass → User Story 5 independently functional.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, README, Doxygen audit, performance benchmark, CLI harness.

- [x] T062 [P] Audit all public headers in `include/homestead/` — every exported type, function, and constant must have a `///` Doxygen comment; add missing comments; verify Doxygen builds without warnings (`doxygen -q Doxyfile`)
- [x] T063 [P] Write `README.md` at repo root with: project description, prerequisites table, build instructions for all 3 presets, FetchContent integration snippet, and the minimal usage example from quickstart.md (SC per constitution Principle IV)
- [x] T064 [P] Create `Doxyfile` at repo root: `INPUT = include/`, `RECURSIVE = YES`, `EXTRACT_ALL = NO` (only documented symbols), `HTML_OUTPUT = docs/`
- [x] T065 Write dedicated performance benchmark in `tests/solver/bench_solver.cpp` (separate from test_properties.cpp per constitution Principle VIII): generate a synthetic plan with 10,000 entity instances spanning 5+ resource cycles; assert solver completes in < 1 second using `std::chrono::high_resolution_clock`; add `bench_solver` as a CTest entry in `tests/solver/CMakeLists.txt`; validates SC-002
- [x] T066 [P] Implement `src/cli/main.cpp` (only compiled when `HOMESTEAD_BUILD_CLI=ON`): parse `--scenario <file>` and `--output <file>` args; deserialize Registry from scenario JSON; run solver; serialize PlanResult to output file; exit 0 on success, 1 on error with message to stderr
- [x] T067 [P] Create `src/cli/CMakeLists.txt`: `homestead_cli` executable, PRIVATE link all four library targets; wrapped in `if(HOMESTEAD_BUILD_CLI)` in top-level CMakeLists.txt
- [x] T068 Run `clang-format --dry-run -Werror` and `clang-tidy` on all source files; fix any reported issues
- [x] T073 [P] Add a small-plan timing assertion to `tests/solver/bench_solver.cpp`: define a scenario with 3 entity instances (chicken coop + compost bin + corn plot), one production goal, and assert the full solve-to-PlanResult round-trip completes in < 100 ms using `std::chrono::high_resolution_clock`; validates SC-001
- [x] T069 [P] Add `quickstart.md` validation: create `tests/solver/test_quickstart.cpp` (not `tests/integration/` — use the existing CTest-registered solver test target) exercising the exact code example from quickstart.md (50 kg chicken + 200 eggs, 200 m² constraint), asserting the plan completes without crash and loop closure score > 0.0

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately; most tasks parallelisable
- **Foundational (Phase 2)**: Depends on Phase 1 completion — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Phase 2; no dependency on US2–US5
- **US2 (Phase 4)**: Depends on Phase 2; no dependency on US1, US3–US5
- **US3 (Phase 5)**: Depends on Phase 2 AND US1 (needs Registry) AND US2 (needs Graph)
- **US4 (Phase 6)**: Depends on US3 (analytics require a solved PlanResult)
- **US5 (Phase 7)**: Depends on US1 (Registry types), US3 (PlanResult type); can start in parallel with US4
- **Polish (Phase 8)**: Depends on all user stories complete

### User Story Dependencies

```
Phase 1 (Setup)
    └─► Phase 2 (Foundational)
            ├─► US1 (Registry + defaults) ────────────────────┐
            └─► US2 (Graph construction) ─────────────────────┤
                                                              ↓
                                                    US3 (Solver) ──► US4 (Analytics)
                                                         │
                                                         └──────────► US5 (Serialization)
                                                                           │
                                                                    Phase 8 (Polish)
```

### Within Each User Story

- Headers before implementation (`.hpp` before `.cpp`)
- Implementation before tests where integration tests require working code
- Unit tests for types can be written in parallel with implementation

### Parallel Opportunities

- All Setup tasks marked [P] can run simultaneously (different files)
- T015–T020 (Foundational type definitions) are fully parallel
- T030–T032 (Graph node/edge types) are parallel
- T037–T039 (Graph tests) are parallel after T033–T036
- T040–T042 (Solver config/result/strategy headers) are parallel
- T047–T051 (Solver integration + property tests) are parallel after T043–T046
- T055, T060–T061 (Serialization schema + roundtrip tests) parallel after T056–T059
- T062–T064, T066–T067, T069 (polish tasks) are fully parallel

---

## Parallel Example: Phase 2 (Foundational)

```bash
# All of these can run simultaneously:
Task T015: implement include/homestead/core/quantity.hpp
Task T016: implement resource type building blocks in include/homestead/core/resource.hpp
Task T018: implement Lifecycle / ResourceFlowSpec / InfrastructureSpec in include/homestead/core/entity.hpp
Task T020: implement RegistryError in include/homestead/core/registry.hpp

# Then sequentially:
Task T017: implement Resource struct (depends on T016)
Task T019: implement Entity struct (depends on T018)
Task T021: implement Registry class (depends on T017, T019, T020)

# Tests can run parallel after their subjects:
Task T022: test_quantity.cpp (after T015)
Task T023: test_resource.cpp (after T017) [P]
Task T024: test_entity.cpp (after T019) [P]
Task T025: test_registry.cpp (after T021)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T014)
2. Complete Phase 2: Foundational types (T015–T021)
3. Complete Phase 3: US1 registry + defaults (T026–T029)
4. **STOP and VALIDATE**: `ctest --preset debug -R test_registry` all pass
5. Default registry queryable — minimal library usable

### Incremental Delivery

1. Phase 1 + 2 → Core types + Registry (T001–T025)
2. Phase 3: US1 → Default registry populated and queryable (T026–T029)
3. Phase 4: US2 → Graph buildable, cycles detectable (T030–T039)
4. Phase 5: US3 → Solver produces PlanResult (T040–T051)
5. Phase 6: US4 → Analytics reports generated (T052–T054)
6. Phase 7: US5 → JSON round-trip (T055–T061)
7. Phase 8 → Polish, docs, CLI, benchmark (T062–T069)

### Parallel Team Strategy

With two developers after Phase 2 is complete:

- Developer A: US1 (T026–T029) → US2 (T030–T039)
- Developer B: (waits for US1 + US2) → US3 (T040–T051)
- Both: US4 + US5 in parallel → Phase 8

---

## Notes

- `[P]` tasks write to different files with no inter-task dependencies
- `[USn]` label maps task to spec.md user story for traceability
- Commit after each checkpoint (end of each phase)
- Run `clang-format` and `clang-tidy` before every commit (constitution Principle V)
- All `std::expected` return types — no exceptions on any path
- Never add a non-stdlib dependency to `homestead::core`; CI will catch violations via target dependency graph
