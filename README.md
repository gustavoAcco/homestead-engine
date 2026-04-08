# homestead-engine

Integrated food-production planning engine. Models resource flows in
self-sustaining homesteads (inspired by Embrapa's **Sisteminha**) as a
directed graph and solves backwards from desired outputs to discover all
required inputs, infrastructure, labour, and external purchases.

This is a **C++ library** — no GUI, no CLI (except an optional test harness),
no web server.

## Prerequisites

| Tool | Minimum version |
|------|----------------|
| CMake | 3.28 |
| C++ compiler | GCC 13 / Clang 17 / MSVC 2022 (C++23 support required) |
| Ninja | any recent |
| Internet connection | FetchContent downloads nlohmann/json, Catch2, spdlog |

## Build

```bash
# Configure + build (debug, tests enabled)
cmake --preset debug
cmake --build --preset debug

# Run all tests
ctest --preset debug

# Release build (no tests)
cmake --preset release
cmake --build --preset release

# Sanitizers (ASan + UBSan)
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

## FetchContent integration

```cmake
include(FetchContent)
FetchContent_Declare(homestead-engine
    GIT_REPOSITORY https://github.com/your-org/homestead-engine.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(homestead-engine)

target_link_libraries(my_app PRIVATE homestead::solver homestead::serialization)
```

## Quick-start example

```cpp
#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>
#include <homestead/solver/result.hpp>
#include <iostream>

int main() {
    // Load the built-in tropical-agriculture registry (20 entities).
    auto registry = homestead::Registry::load_defaults();

    // Define production goals: 50 kg chicken + 200 eggs per month.
    std::vector<homestead::ProductionGoal> goals = {
        {"broiler_meat_kg", homestead::VariableQuantity{50.0}},
        {"egg",             homestead::VariableQuantity{200.0}},
    };

    // Constrain to 200 m².
    homestead::SolverConfig config;
    config.max_area_m2 = 200.0;

    // Solve.
    auto plan = homestead::solve(goals, registry, config);

    std::cout << "Loop closure: " << plan.loop_closure_score * 100 << " %\n";
    for (const auto& d : plan.diagnostics)
        std::cout << "[" << homestead::to_string(d.kind) << "] " << d.message << "\n";
}
```

## Library targets

| Target | Purpose | External dependencies |
|--------|---------|----------------------|
| `homestead::core` | Domain model (Resource, Entity, Registry, quantities) | stdlib only |
| `homestead::graph` | Directed resource-flow graph, cycle detection | core |
| `homestead::solver` | Backpropagation planner, scheduling, analytics | core, graph |
| `homestead::serialization` | JSON import/export, schema versioning | core, nlohmann/json |
| `homestead_cli` | Minimal CLI harness (opt-in via `HOMESTEAD_BUILD_CLI=ON`) | all above |

## Architecture principles

- Value semantics throughout — no raw `new`/`delete`
- `std::expected<T,E>` for error handling; no exceptions in the solver
- Circular dependencies (chicken manure → compost → corn → feed → chicken) are
  handled by Gauss-Seidel fixed-point convergence, not assumed away
- All quantities use a `(min, expected, max)` triple
- JSON schemas include a `"version"` field; breaking changes require a MAJOR bump
