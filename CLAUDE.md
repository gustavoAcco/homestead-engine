# homestead-engine

Integrated food production planning engine. Models resource flows in self-sustaining homesteads (inspired by Embrapa's Sisteminha) as a directed graph and solves backwards from desired outputs to discover all required inputs, infrastructure, labor, and external purchases.

This is a **C++ library** — no GUI, no CLI (except a minimal test harness), no web server.

## Build

```bash
cmake --preset debug       # configure
cmake --build --preset debug  # build
ctest --preset debug          # test
```

Presets are defined in `CMakePresets.json`: `debug`, `release`, `sanitize`.

## Tech stack

- C++23, CMake 3.28+
- nlohmann/json (serialization), Catch2 v3 (testing), spdlog (optional logging)
- All dependencies via FetchContent — no system-level installs needed

## Architecture

Five library targets with strict dependency boundaries:

| Target | Purpose | Depends on |
|---|---|---|
| `homestead::core` | Domain model (Resource, Entity, Registry, quantities, lifecycle) | std only |
| `homestead::graph` | Directed resource-flow graph, traversal, cycle detection | core |
| `homestead::solver` | Backpropagation planner, scheduling, optimization | core, graph |
| `homestead::serialization` | JSON import/export, schema versioning | core, nlohmann/json |
| `homestead::cli` | Minimal test CLI (off by default) | all above |

Public headers: `include/homestead/<module>/`. Sources: `src/<module>/`. Tests: `tests/<module>/`.

## Code style

- **snake_case** for functions and variables, **PascalCase** for types and concepts, **UPPER_SNAKE** for macros
- Follow the C++ Core Guidelines
- clang-format and clang-tidy configs are at the repo root — always run them before committing
- Prefer value semantics. Use `std::expected` or `std::optional` for error handling — no exceptions in the solver hot path
- Use concepts to constrain templates. Use ranges/views where they simplify code
- No raw `new`/`delete`. Prefer `std::unique_ptr` or stack allocation

## Testing

- Every public API must have unit tests (Catch2 v3)
- Run a single test file: `ctest --preset debug -R <test_name>`
- Run all tests: `ctest --preset debug`
- Solver integration tests live in `tests/solver/` and use hand-calculated expected results
- Property-based tests: assert invariants like "total internal production >= total internal consumption" on randomly generated entity configurations

## Important rules

- **Never add dependencies to `homestead::core`** — it must compile with only the C++ standard library
- **Never put I/O in domain types** — serialization is a separate target for a reason
- The solver must handle circular resource dependencies (chicken manure → compost → corn → feed → chicken) via iterative fixed-point convergence — never assume the graph is a DAG
- All quantities use a `(min, expected, max)` triple, not a single value
- JSON schemas must include a `"version"` field — breaking changes require a version bump
- When compacting, preserve: the current feature branch name, which solver tests are passing/failing, and the list of modified files

## Additional context

Read these files when relevant to your current task:

- `.specify/memory/constitution.md` — project principles and development guidelines
- `.specify/specs/*/spec.md` — feature specifications
- `.specify/specs/*/plan.md` — technical implementation plans
- `data/default_registry.json` — the starter database of entities and resources (tropical agriculture focus)

## Active Technologies
- C++23 (std::expected, concepts, ranges, std::span) + nlohmann/json 3.11+, Catch2 v3.6+, spdlog 1.13+ (optional) (001-homestead-engine-core)
- JSON files for Registry and PlanResult persistence; no database (001-homestead-engine-core)
- C++23 (GCC 13+, Clang 17+, MSVC latest) + nlohmann/json 3.11+, Catch2 v3.6+ (testing), spdlog 1.13+ (optional logging) — all via FetchContent (002-quantity-scaling-solver)
- N/A (in-memory graph; no persistence changes in this feature) (002-quantity-scaling-solver)
- C++23 (GCC 13+, Clang 17+, MSVC latest) + nlohmann/json 3.11+ (serialization), Catch2 v3.6+ (testing) (003-npk-soil-balance)

## Recent Changes
- 001-homestead-engine-core: Added C++23 (std::expected, concepts, ranges, std::span) + nlohmann/json 3.11+, Catch2 v3.6+, spdlog 1.13+ (optional)
