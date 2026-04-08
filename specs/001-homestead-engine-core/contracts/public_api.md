# Public API Contract: homestead-engine Core Library

**Branch**: `001-homestead-engine-core` | **Date**: 2026-04-07

This document describes the public API surface that downstream consumers
(`homestead-api`, `homestead-web`, and third-party integrators) depend on.
Anything listed here is subject to SemVer compatibility guarantees. Internal
implementation files under `src/` are not part of this contract.

---

## Target: homestead::core

Consumed via: `find_package(homestead COMPONENTS core)` or `add_subdirectory`.

### Registry — primary entry point

```cpp
#include <homestead/core/registry.hpp>

// Load the built-in default registry (tropical agriculture).
// Never fails — defaults are embedded at compile time.
homestead::Registry reg = homestead::Registry::load_defaults();

// Add a custom resource (eager slug validation).
auto result = reg.register_resource(homestead::Resource{
    .slug     = "my_biochar_kg",
    .name     = "Biochar",
    .category = homestead::ResourceCategory::fertilizer,
    .physical = { .weight_kg_per_unit = 1.0, .volume_liters_per_unit = 2.0,
                  .shelf_life_days = -1 }
});
if (!result) { /* result.error() is a RegistryError */ }

// Add a custom entity (eager validation: all resource slugs must already exist).
auto e_result = reg.register_entity(homestead::Entity{ ... });

// Query
auto res = reg.find_resource("chicken_manure_kg");  // std::optional<Resource>
auto ent = reg.find_entity("broiler_chicken");       // std::optional<Entity>

// Find which entities can produce a resource
std::vector<std::string> slugs = reg.producers_of("corn_grain_kg");
```

### VariableQuantity

```cpp
#include <homestead/core/quantity.hpp>

homestead::VariableQuantity q1{5.0};              // fixed: min=expected=max=5
homestead::VariableQuantity q2{3.0, 5.0, 7.0};   // range

double val = q2.expected;   // 5.0
double val = q2.min;        // 3.0
```

---

## Target: homestead::graph

Consumed via: `find_package(homestead COMPONENTS graph)`.
Depends on `homestead::core` (transitively available to consumer).

```cpp
#include <homestead/graph/graph.hpp>

homestead::Graph g;

homestead::NodeId coop = g.add_entity_instance({
    .instance_name = "my_chicken_coop",
    .entity_slug   = "broiler_chicken",
    .quantity      = homestead::VariableQuantity{50.0},
    .schedule      = homestead::ALL_MONTHS
});

homestead::NodeId bin = g.add_entity_instance({
    .instance_name = "my_compost_bin",
    .entity_slug   = "compost_bin",
    .quantity      = homestead::VariableQuantity{1.0},
    .schedule      = homestead::ALL_MONTHS
});

homestead::NodeId sink = g.add_goal_sink({
    .resource_slug      = "broiler_meat_kg",
    .quantity_per_month = homestead::VariableQuantity{50.0}
});

auto flow_result = g.add_flow({
    .from              = coop,
    .to                = bin,
    .resource_slug     = "chicken_manure_kg",
    .quantity_per_cycle = homestead::VariableQuantity{1.0, 1.5, 2.0}
});

// Cycle detection
bool cyclic = g.has_cycle();
auto cycles = g.find_cycles();  // std::vector<std::vector<NodeId>>

// Unsatisfied inputs (gaps before solving)
auto gaps = g.unsatisfied_inputs(reg);  // vector<pair<NodeId, string>>
```

---

## Target: homestead::solver

Consumed via: `find_package(homestead COMPONENTS solver)`.
Depends on `homestead::core` and `homestead::graph` (transitively available).

### Simple usage (free function)

```cpp
#include <homestead/solver/solver.hpp>

homestead::Registry reg = homestead::Registry::load_defaults();

std::vector<homestead::ProductionGoal> goals = {
    { .resource_slug = "broiler_meat_kg",
      .quantity_per_month = homestead::VariableQuantity{50.0} },
    { .resource_slug = "egg",
      .quantity_per_month = homestead::VariableQuantity{200.0} },
};

homestead::SolverConfig cfg;
cfg.max_area_m2 = 500.0;

homestead::PlanResult plan = homestead::solve(goals, reg, cfg);

// Check for partial satisfaction
if (!plan.diagnostics.empty()) {
    for (const auto& d : plan.diagnostics) {
        // d.kind, d.message, d.resource_slug, d.entity_slug
    }
}

// Loop closure score
double pct = plan.loop_closure_score * 100.0;  // e.g. 78.3%

// Resource balance sheet (monthly breakdown)
for (const auto& rb : plan.balance_sheet) {
    // rb.resource_slug
    // rb.internal_production[0..11]  (Jan–Dec)
    // rb.external_purchase[0..11]
    // rb.annual_internal_production
}

// Labor schedule
for (int m = 0; m < 12; ++m) {
    double hours = plan.labor_schedule[m];
}

// Infrastructure BOM
double area = plan.bom.total_area_m2;
double cost = plan.bom.estimated_initial_cost;
```

### Strategy pattern (future LP/MIP backend)

```cpp
#include <homestead/solver/strategy.hpp>

class MyCustomSolver : public homestead::ISolverStrategy {
public:
    homestead::PlanResult solve(
        std::span<const homestead::ProductionGoal> goals,
        const homestead::Registry& registry,
        const homestead::SolverConfig& config) override { ... }
};

MyCustomSolver solver;
homestead::PlanResult plan = solver.solve(goals, reg, cfg);
```

---

## Target: homestead::serialization

Consumed via: `find_package(homestead COMPONENTS serialization)`.
Depends on `homestead::core` and `nlohmann::json` (transitively available).

```cpp
#include <homestead/serialization/serialization.hpp>
#include <fstream>

homestead::Registry reg = homestead::Registry::load_defaults();

// Serialize to JSON
nlohmann::json j = homestead::to_json(reg);
std::ofstream("registry.json") << j.dump(2);

// Deserialize from JSON
std::ifstream f("registry.json");
nlohmann::json loaded = nlohmann::json::parse(f);
auto result = homestead::registry_from_json(loaded);
if (!result) {
    std::cerr << result.error() << '\n';  // descriptive error string
}

// Serialize a solved plan
homestead::PlanResult plan = homestead::solve(goals, reg);
nlohmann::json plan_json = homestead::to_json(plan);

// Deserialize a plan (does not require re-solving)
auto plan_result = homestead::plan_from_json(plan_json);
```

### JSON document structure

Every document produced by `to_json` follows this envelope:

```json
{
  "version": "1.0.0",
  "type": "registry",
  "data": { ... }
}
```

`type` is one of: `"registry"`, `"graph"`, `"plan_result"`.

Deserialization rejects documents where `version`'s MAJOR component differs from
the library's supported MAJOR (currently `1`).

---

## CMake Integration (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(homestead
  GIT_REPOSITORY https://github.com/your-org/homestead-engine.git
  GIT_TAG        v0.1.0)
FetchContent_MakeAvailable(homestead)

target_link_libraries(my_app PRIVATE homestead::solver homestead::serialization)
```

Or as a subdirectory:

```cmake
add_subdirectory(third_party/homestead-engine)
target_link_libraries(my_app PRIVATE homestead::solver homestead::serialization)
```

Consumers do **not** need to separately link `homestead::core` or `homestead::graph`
— they are transitively pulled in via `INTERFACE` / `PUBLIC` linkage.

---

## Error Handling Contract

| Layer | Failure mode | Return type |
|---|---|---|
| `Registry::register_*` | Validation failure | `std::expected<void, RegistryError>` |
| `Graph::add_flow` / `remove_node` | Unknown node, self-loop | `std::expected<void, GraphError>` |
| `solve()` | Never fails | `PlanResult` (diagnostics list carries failures) |
| `*_from_json()` | Bad version, missing field, wrong type | `std::expected<T, std::string>` |

**The solver never returns an error result and never throws.** Partial satisfaction
is communicated entirely through `PlanResult::diagnostics`.

---

## Stability Guarantees

- **Stable (SemVer protected)**: All types and free functions in `include/homestead/`.
- **Unstable**: Everything under `src/` (implementation detail).
- **Intentionally excluded from public API**: `homestead::cli` target, the
  `HOMESTEAD_LOG_*` macros (private to implementation files), internal solver
  helper classes in `src/solver/`.
