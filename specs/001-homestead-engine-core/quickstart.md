# Quick Start: homestead-engine

**Branch**: `001-homestead-engine-core` | **Date**: 2026-04-07

---

## Prerequisites

| Tool | Minimum version |
|---|---|
| CMake | 3.28 |
| GCC | 13 (Linux) |
| Clang | 17 (Linux/macOS) |
| MSVC | 19.38 / VS 2022 17.8 (Windows) |
| Git | any recent |
| Internet access | Required on first build (FetchContent downloads deps) |

---

## Build

```bash
# Clone
git clone https://github.com/your-org/homestead-engine.git
cd homestead-engine

# Configure (debug preset — tests ON, CLI OFF, logging OFF)
cmake --preset debug

# Build
cmake --build --preset debug

# Run all tests
ctest --preset debug

# Run a single test file
ctest --preset debug -R test_registry

# Build with address + undefined-behaviour sanitizers
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

Available presets (`CMakePresets.json`):

| Preset | Flags | Use for |
|---|---|---|
| `debug` | `-O0 -g`, tests ON | Day-to-day development |
| `release` | `-O3 -DNDEBUG`, tests OFF | Benchmarking / distribution |
| `sanitize` | ASan + UBSan, tests ON | Memory/UB validation |

Optional CMake options:

```bash
# Enable the minimal CLI harness
cmake --preset debug -DHOMESTEAD_BUILD_CLI=ON

# Enable spdlog-based logging in solver
cmake --preset debug -DHOMESTEAD_ENABLE_LOGGING=ON
```

---

## Consume as a dependency

**Via FetchContent** (recommended):

```cmake
include(FetchContent)
FetchContent_Declare(homestead
  GIT_REPOSITORY https://github.com/your-org/homestead-engine.git
  GIT_TAG        v0.1.0)
FetchContent_MakeAvailable(homestead)

target_link_libraries(my_app PRIVATE homestead::solver homestead::serialization)
```

**Via add_subdirectory**:

```cmake
add_subdirectory(third_party/homestead-engine)
target_link_libraries(my_app PRIVATE homestead::solver homestead::serialization)
```

---

## Minimal usage example

Plan a homestead that produces 50 kg of broiler chicken meat per month and
200 eggs per month, with a 200 m² area constraint.

```cpp
#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>
#include <homestead/serialization/serialization.hpp>
#include <iostream>
#include <fstream>

int main() {
    // 1. Load the default tropical-agriculture registry
    auto reg = homestead::Registry::load_defaults();

    // 2. Define production goals
    using homestead::VariableQuantity;
    std::vector<homestead::ProductionGoal> goals = {
        { "broiler_meat_kg",  VariableQuantity{50.0}  },
        { "egg",              VariableQuantity{200.0} },
    };

    // 3. Configure constraints
    homestead::SolverConfig cfg;
    cfg.max_area_m2 = 200.0;
    cfg.scenario    = homestead::SolverConfig::Scenario::expected;

    // 4. Solve
    homestead::PlanResult plan = homestead::solve(goals, reg, cfg);

    // 5. Inspect diagnostics (partial satisfaction)
    if (!plan.diagnostics.empty()) {
        std::cout << "Warnings:\n";
        for (const auto& d : plan.diagnostics) {
            std::cout << "  [" << homestead::to_string(d.kind) << "] "
                      << d.message << '\n';
        }
    }

    // 6. Loop closure score
    std::cout << "Loop closure: "
              << plan.loop_closure_score * 100.0 << "%\n";

    // 7. Area required
    std::cout << "Total area:   "
              << plan.bom.total_area_m2 << " m²\n";

    // 8. Monthly labor for January
    std::cout << "Jan labor:    "
              << plan.labor_schedule[0] << " hours\n";

    // 9. Resource balance for corn (monthly)
    for (const auto& rb : plan.balance_sheet) {
        if (rb.resource_slug == "corn_grain_kg") {
            std::cout << "Corn produced (annual): "
                      << rb.annual_internal_production << " kg\n";
            std::cout << "Corn purchased (annual): "
                      << rb.annual_external_purchase << " kg\n";
        }
    }

    // 10. Save the plan to disk
    nlohmann::json j = homestead::to_json(plan);
    std::ofstream("plan.json") << j.dump(2);

    // 11. Reload the plan (no re-solving needed)
    std::ifstream f("plan.json");
    auto loaded = homestead::plan_from_json(nlohmann::json::parse(f));
    if (!loaded) {
        std::cerr << "Failed to load plan: " << loaded.error() << '\n';
        return 1;
    }

    return 0;
}
```

Expected output (approximate, using default registry expected values):

```
Loop closure: 67.2%
Total area:   148.5 m²
Jan labor:    12.4 hours
Corn produced (annual): 96.0 kg
Corn purchased (annual): 42.0 kg
```

---

## Adding a custom entity

```cpp
// All resource slugs referenced in inputs/outputs must exist in the registry first.
auto result = reg.register_resource(homestead::Resource{
    .slug     = "black_soldier_fly_larvae_kg",
    .name     = "Black Soldier Fly Larvae",
    .category = homestead::ResourceCategory::feed,
    .nutrition = homestead::NutritionalProfile{
        .protein_g_per_kg = 420.0,
        .fat_g_per_kg     = 290.0,
    },
    .physical = { .weight_kg_per_unit = 1.0, .shelf_life_days = 3 }
});

auto entity_result = reg.register_entity(homestead::Entity{
    .slug        = "bsf_bin",
    .name        = "Black Soldier Fly Bin",
    .description = "Converts organic waste to high-protein larvae",
    .inputs  = {
        { "organic_waste_kg", homestead::VariableQuantity{10.0, 12.0, 15.0}, 0.0 },
        { "human_labor_hours", homestead::VariableQuantity{0.5}, 0.5 },
    },
    .outputs = {
        { "black_soldier_fly_larvae_kg", homestead::VariableQuantity{1.0, 1.3, 1.6}, 1.0 },
        { "bsf_frass_kg", homestead::VariableQuantity{3.0, 3.5, 4.0}, 1.0 },
    },
    .lifecycle = {
        .setup_days    = 7,
        .cycle_days    = 14,
        .cycles_per_year = 26.0,
        .active_months = homestead::ALL_MONTHS
    },
    .operating_labor_per_cycle = homestead::VariableQuantity{0.5, 0.7, 1.0},
    .stocking_density = homestead::VariableQuantity{50.0},
    .infrastructure = {
        .area_m2             = homestead::VariableQuantity{0.5, 1.0, 2.0},
        .initial_labor_hours = homestead::VariableQuantity{4.0},
        .initial_cost        = homestead::VariableQuantity{50.0, 80.0, 120.0},
    }
});
// entity_result is std::expected<void, RegistryError>
```

---

## Running the CLI harness (optional)

Build with `HOMESTEAD_BUILD_CLI=ON`, then:

```bash
# Run the solver on a JSON scenario file
./build/debug/homestead_cli --scenario my_scenario.json --output plan.json

# Scenario file format (subset of Registry + goals)
# See data/default_registry.json for the resource/entity format.
```
