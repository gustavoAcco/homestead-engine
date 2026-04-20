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

    // Define production goals: 50 kg chicken + 200 eggs + 30 kg corn per month.
    // Including a corn goal creates internal grain surplus that the solver
    // automatically routes to meet the broiler flock's protein and energy needs.
    std::vector<homestead::ProductionGoal> goals = {
        {"broiler_meat_kg", homestead::VariableQuantity{50.0}},
        {"egg",             homestead::VariableQuantity{200.0}},
        {"corn_grain_kg",   homestead::VariableQuantity{30.0}},
    };

    // Constrain to 200 m².
    homestead::SolverConfig config;
    config.max_area_m2 = 200.0;

    // Solve.
    auto plan = homestead::solve(goals, registry, config);

    // Inspect entity quantities.
    plan.graph.for_each_node([](homestead::NodeId, const homestead::NodeVariant& v) {
        if (auto* inst = std::get_if<homestead::EntityInstanceNode>(&v)) {
            std::cout << inst->entity_slug << ": " << inst->quantity.expected << " instances\n";
        }
    });

    std::cout << "Area:          " << plan.bom.total_area_m2         << " m²\n";
    std::cout << "Setup cost:    " << plan.bom.estimated_initial_cost << " BRL\n";
    std::cout << "Loop closure:  " << plan.loop_closure_score * 100   << " %\n";

    for (const auto& d : plan.diagnostics)
        std::cout << "[" << homestead::to_string(d.kind) << "] " << d.message << "\n";
}
```

The loop closure score rises as more of the system's demand is met internally.
Adding a corn goal creates grain surplus that the composition-matching pass
automatically routes to feed the broiler flock — no explicit feed slug needed.
Without any grain goal the solver emits `composition_gap` diagnostics and
purchases protein and energy externally, keeping the score low.

## Solver config

```cpp
homestead::SolverConfig config;
config.scenario                   = homestead::Scenario::expected;  // optimistic | expected | pessimistic
config.max_area_m2                = 500.0;   // optional hard cap (m²)
config.max_budget                 = 8000.0;  // optional hard cap (BRL)
config.max_labor_hours_per_month  = 40.0;    // optional hard cap (h/month)
config.max_convergence_iterations = 100;     // Gauss-Seidel topology cap
config.max_quantity_iterations    = 50;      // quantity fixed-point cap
```

`Scenario` selects which point of every `VariableQuantity{min, expected, max}` triple
the solver uses for its calculations. All three scenario points are stored in the result
graph so callers can report pessimistic/expected/optimistic ranges from a single solve.

## Plan result

`homestead::PlanResult` contains everything produced by one solver run:

| Field | Type | Description |
|-------|------|-------------|
| `graph` | `Graph` | Fully resolved resource-flow graph. Nodes: entity instances, external sources, goal sinks. Edges: `ResourceFlow` values. |
| `balance_sheet` | `vector<ResourceBalance>` | Per-resource monthly production, consumption, and external purchase across all 12 months. Each entry also carries `composition_routed` — grams of each element (N_g, P_g, K_g, protein_g, …) routed through that resource by the composition-matching passes. |
| `gap_report` | `vector<ResourceBalance>` | Subset of `balance_sheet` where a resource has unmet internal demand (purchased externally). Includes synthetic entries such as `protein_g_external` when elemental needs go unmet. |
| `labor_schedule` | `MonthlyValues` | Total operating labour hours per month across all entity instances. |
| `bom` | `InfrastructureBOM` | Total area (m²), estimated setup cost, setup labour, and construction materials. |
| `loop_closure_score` | `double ∈ [0,1]` | Fraction of total resource demand met internally, including gram-level composition flows (N_g, P_g, K_g, protein_g — energy_kcal excluded). 1.0 = fully self-sufficient. |
| `nutrient_balance` | `optional<NutrientBalance>` | Per-month N/P/K supply vs. crop demand (nullopt when no crop entities are present). |
| `diagnostics` | `vector<Diagnostic>` | Solver messages. Empty implies all goals fully satisfied. |

### Diagnostic kinds

| Kind | Meaning |
|------|---------|
| `missing_producer` | No registry entity produces a required resource — routed to external source |
| `area_exceeded` | An entity was skipped because it would breach `max_area_m2` |
| `unsatisfied_goal` | A goal cannot be met due to budget constraint or no producer |
| `non_convergent_cycle` | Quantity fixed-point did not converge within `max_quantity_iterations` |
| `seasonality_gap` | A goal resource is unavailable in one or more months of the year |
| `labor_constraint` | Total labour exceeds `max_labor_hours_per_month` in one or more months |
| `nutrient_deficit` | N, P, or K supply falls short of crop demand in one or more months |
| `composition_gap` | An elemental requirement (protein_g, energy_kcal, N_g, P_g, K_g) is partially or fully unmet by internal supply — remainder purchased externally. `Diagnostic::shortfall_g` carries the unmet grams. |

### Traversing the graph

```cpp
// Iterate every node regardless of type.
plan.graph.for_each_node([](homestead::NodeId id, const homestead::NodeVariant& v) {
    std::visit([](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, homestead::EntityInstanceNode>)
            std::cout << node.entity_slug << " x" << node.quantity.expected << "\n";
    }, v);
});

// Predecessor/successor traversal.
auto preds = plan.graph.predecessors(node_id);
auto succs = plan.graph.successors(node_id);
```

`EntityInstanceNode::quantity` is a `VariableQuantity{N, N, N}` triple where `N` is the
ceiling of `(monthly_demand / output_per_month_per_instance)` — computed automatically
by the solver; never hardcoded to 1.

## CLI

The optional CLI harness is built when `HOMESTEAD_BUILD_CLI=ON` (it is on by default in
the debug preset). It is a thin wrapper around the library — not part of the public API.

```bash
# Solve using the built-in registry and a goals file.
homestead_cli --defaults --goals data/sisteminha_goals.json --output plan.json

# Solve using a custom registry file.
homestead_cli --scenario registry.json --goals goals.json --output plan.json

# Export the default registry to JSON for inspection or customisation.
homestead_cli --dump-defaults --output registry.json
```

The output `plan.json` is a versioned JSON document (`"type": "plan_result"`,
`"version": "1.2.0"`) readable by `homestead::serialization::plan_from_json`.

## Data files

| File | Description |
|------|-------------|
| `data/default_registry.json` | Serialised form of the built-in registry. 20 entities and 38+ resources modelling a tropical mixed-farming system (broiler chickens, laying hens, quail, tilapia, goats, vegetable beds, fruit trees, compost, earthworm bin, biodigester, water infrastructure). Animal entities declare nutritional needs via `composition_requirements`; area-based crop entities declare NPK needs via `fertilization_per_m2`. Feed resources carry `protein_g` and `energy_kcal` composition keys; fertiliser resources carry `N_g`, `P_g`, `K_g` keys for solver-time matching. |
| `data/sisteminha_goals.json` | Example goal set for a Sisteminha-scale homestead: 8 food goals ranging from 20 to 200 units/month. Pass to `--goals`. |

## Library targets

| Target | Purpose | External dependencies |
|--------|---------|----------------------|
| `homestead::core` | Domain model: `Resource`, `Entity`, `Registry`, `VariableQuantity`, `Lifecycle`, `MonthMask` | stdlib only |
| `homestead::graph` | Directed resource-flow graph, cycle detection, `for_each_node` traversal | core |
| `homestead::solver` | Backpropagation planner, quantity scaling, scheduling, analytics, composition matching | core, graph |
| `homestead::serialization` | JSON import/export, schema versioning | core, nlohmann/json |
| `homestead_cli` | Minimal CLI harness (opt-in via `HOMESTEAD_BUILD_CLI=ON`) | all above |

## How the solver works

1. **Seed**: a `GoalSinkNode` is created for each production goal and seeded into the demand queue with `quantity_per_month = pick(goal.quantity_per_month, scenario)`.
2. **Expand**: for each demand, find a registry producer. Compute the required number of instances: `ceil(demand_per_month / (output_per_cycle × cycles_in_month))`. Add an `EntityInstanceNode` with that quantity, connect it, and enqueue all of its declared inputs scaled by the instance count.
3. **Reuse**: if an entity node already exists, accumulate the new demand and re-enqueue its inputs with the incremental delta if the required quantity grew.
4. **Converge**: after processing all demands, recheck `unsatisfied_inputs()`. Repeat (Gauss-Seidel outer loop) until stable or the iteration cap is reached. This correctly handles circular dependencies such as `chicken → manure → compost → crops → feed → chicken`.
5. **Analyse**: compute balance sheet, BOM, labour schedule, N-P-K nutrient balance, and an initial loop closure score from the resolved graph.
6. **Composition matching** (post-analytics, read-only):
   - *Pass 1 — feed matching*: for each entity instance with `composition_requirements`, compute annual elemental demand (protein_g, energy_kcal, N_g, …) and greedily allocate available internal resource surpluses. The allocation quantity satisfies the most-demanding element simultaneously, crediting all others. Unmet remainder creates a synthetic `"{element}_external"` balance entry and a `composition_gap` diagnostic.
   - *Pass 2 — fertilisation matching*: for each entity instance with `fertilization_per_m2`, compute area-scaled annual N/P/K demand and route available N/P/K-bearing resources (manure, vermicompost, nutrient water, …) identically. Unmet per-element remainder → external purchase + diagnostic.
   - Loop closure score is recomputed after both passes to include gram-level composition flows.

Neither composition pass adds entity instances — they only update `ResourceBalance::composition_routed` and emit diagnostics.

All quantities use a `(min, expected, max)` triple throughout. The solver operates on
one scenario point at a time; the full triple is preserved in the output.

## Architecture principles

- Value semantics throughout — no raw `new`/`delete`
- `std::expected<T,E>` for error handling; no exceptions in the solver hot path
- Concepts constrain all templates; ranges/views used where they simplify code
- Circular dependencies handled by Gauss-Seidel fixed-point convergence, not assumed away
- All quantities use a `(min, expected, max)` triple; never a single scalar
- JSON schemas include a `"version"` field; breaking changes require a MAJOR bump
- `homestead::core` has zero external dependencies — only the C++ standard library
