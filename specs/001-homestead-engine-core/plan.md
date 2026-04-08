# Implementation Plan: homestead-engine Core Library

**Branch**: `001-homestead-engine-core` | **Date**: 2026-04-07 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/001-homestead-engine-core/spec.md`

## Summary

Build a C++23 static library that models an integrated food-production homestead
as a directed resource-flow graph and solves backwards from desired outputs to a
complete, scheduled production plan. The library is organized into five CMake
targets with strict one-way dependency boundaries: `homestead::core` (stdlib-only
domain model), `homestead::graph` (directed graph + cycle detection),
`homestead::solver` (backpropagation planner with fixed-point convergence),
`homestead::serialization` (JSON I/O via nlohmann/json), and `homestead::cli`
(optional test harness). The solver always returns a `PlanResult` paired with a
`Diagnostics` list — it never aborts. Every public API surface is covered by
Catch2 v3 unit tests; solver invariants are validated by property-based tests.

## Technical Context

**Language/Version**: C++23 (std::expected, concepts, ranges, std::span)
**Primary Dependencies**: nlohmann/json 3.11+, Catch2 v3.6+, spdlog 1.13+ (optional)
**Storage**: JSON files for Registry and PlanResult persistence; no database
**Testing**: Catch2 v3 (unit + property-based generators); ctest --preset debug
**Target Platform**: Linux (GCC 13+, Clang 17+), Windows (MSVC latest), macOS — CI matrix
**Project Type**: C++ static library; no GUI, no server
**Performance Goals**: Solver handles 10,000+ entity instances in <1 s; small plan (<50 entities) in <100 ms
**Constraints**: homestead::core has ZERO non-stdlib dependencies; no Boost; no raw new/delete
**Scale/Scope**: Default registry ~20 entity templates, ~60 resources; user plans ~5–200 entity instances

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Modern C++23 Idioms | ✅ PASS | `std::expected` for all fallible ops; concepts on templates; value semantics throughout |
| II. Separation of Concerns | ✅ PASS | `homestead::core` stdlib-only; no I/O in domain types; strict one-way dep graph |
| III. Testing (NON-NEGOTIABLE) | ✅ PASS | Catch2 v3 unit tests for all public APIs; property-based tests for solver; hand-calc integration tests |
| IV. Documentation | ✅ PASS | Doxygen on all `include/homestead/` headers; README with build + usage required |
| V. Code Style | ✅ PASS | clang-format + clang-tidy enforced; `snake_case`/`PascalCase`/`UPPER_SNAKE` convention |
| VI. Build System | ✅ PASS | CMake 3.28+ target-based; `PUBLIC`/`PRIVATE`/`INTERFACE` on all targets; FetchContent for deps |
| VII. CI | ✅ PASS | GitHub Actions matrix (GCC 13+, Clang 17+, MSVC); ASan+UBSan on Linux sanitize preset |
| VIII. Performance | ✅ PASS | 10k-node benchmark required; profile-first discipline; fixed-point convergence bounded |

No violations. Complexity Tracking not required.

## Project Structure

### Documentation (this feature)

```text
specs/001-homestead-engine-core/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   └── public_api.md
└── tasks.md             # Phase 2 output (/speckit-tasks)
```

### Source Code (repository root)

```text
CMakeLists.txt               # Top-level: options, FetchContent, add_subdirectory
CMakePresets.json            # debug / release / sanitize
.clang-format
.clang-tidy
README.md

include/homestead/
├── core/
│   ├── resource.hpp         # Resource, ResourceCategory, ChemicalComposition,
│   │                        #   NutritionalProfile, PhysicalProperties
│   ├── entity.hpp           # Entity, ResourceFlowSpec, Lifecycle, InfrastructureSpec
│   ├── quantity.hpp         # VariableQuantity, MonthMask, ALL_MONTHS
│   └── registry.hpp         # Registry, RegistryError
├── graph/
│   ├── node.hpp             # EntityInstanceNode, ExternalSourceNode, GoalSinkNode,
│   │                        #   NodeId, NodeKind
│   ├── edge.hpp             # ResourceFlow
│   └── graph.hpp            # Graph, GraphError
├── solver/
│   ├── config.hpp           # SolverConfig, ProductionGoal
│   ├── result.hpp           # PlanResult, ResourceBalance, MonthlyValues,
│   │                        #   InfrastructureBOM, Diagnostic, DiagnosticKind
│   ├── strategy.hpp         # ISolverStrategy (pure interface)
│   └── solver.hpp           # BackpropagationSolver, solve() free function
└── serialization/
    ├── schema_version.hpp   # SchemaVersion
    └── serialization.hpp    # to_json / from_json overloads

src/
├── core/
│   ├── CMakeLists.txt
│   └── registry.cpp
├── graph/
│   ├── CMakeLists.txt
│   └── graph.cpp
├── solver/
│   ├── CMakeLists.txt
│   ├── backpropagation.cpp  # Main solver algorithm
│   ├── convergence.cpp      # Fixed-point loop
│   ├── scheduling.cpp       # Monthly time-step alignment
│   └── analytics.cpp        # Balance sheet, gap report, BOM, loop score
└── serialization/
    ├── CMakeLists.txt
    └── serialization.cpp

data/
└── default_registry.json    # Embedded via CMake configure_file or incbin

tests/
├── core/
│   ├── CMakeLists.txt
│   ├── test_resource.cpp
│   ├── test_entity.cpp
│   ├── test_quantity.cpp
│   └── test_registry.cpp
├── graph/
│   ├── CMakeLists.txt
│   ├── test_graph_construction.cpp
│   ├── test_cycle_detection.cpp
│   └── test_traversal.cpp
├── solver/
│   ├── CMakeLists.txt
│   ├── test_backpropagation.cpp    # integration: hand-calculated scenarios
│   ├── test_convergence.cpp        # circular dependency resolution
│   ├── test_scheduling.cpp         # seasonal alignment
│   ├── test_constraints.cpp        # area / labor / budget limits
│   ├── test_analytics.cpp          # balance sheet, loop score
│   └── test_properties.cpp         # property-based invariant tests
└── serialization/
    ├── CMakeLists.txt
    ├── test_roundtrip.cpp
    └── test_schema_version.cpp

src/cli/                            # HOMESTEAD_BUILD_CLI=OFF by default
├── CMakeLists.txt
└── main.cpp
```

**Structure Decision**: Single-project layout. Five library targets under `src/<module>/`
each with their own `CMakeLists.txt`. Public headers under `include/homestead/<module>/`.
Tests mirror the module structure under `tests/<module>/`. The CLI is a separate optional
subdirectory that only gets added when `HOMESTEAD_BUILD_CLI=ON`.

## Complexity Tracking

No constitution violations requiring justification.
