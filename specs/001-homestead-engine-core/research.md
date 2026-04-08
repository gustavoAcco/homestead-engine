# Research: homestead-engine Core Library

**Branch**: `001-homestead-engine-core` | **Date**: 2026-04-07

## 1. Backpropagation Solver Algorithm

**Decision**: Demand-driven backward expansion with iterative fixed-point convergence
for circular dependencies.

**Algorithm sketch**:
```
function solve(goals, registry, config):
  work_queue ← goals
  plan ← empty PlanResult
  seen ← {}

  while work_queue not empty:
    demand ← work_queue.pop()
    if demand.resource in seen: continue   # already being provided

    producers ← registry.producers_of(demand.resource)
    if producers is empty:
      mark demand as external purchase
      continue

    best ← select_by_loop_closure_contribution(producers, plan)
    instance ← instantiate(best, quantity_to_meet(demand, best, config.scenario))
    plan.add(instance)
    seen.add(demand.resource)

    for each input of instance.entity:
      work_queue.push(Demand{input.resource_slug, scaled_quantity})

  # Fixed-point pass for circular dependencies
  repeat until stable (or max_convergence_iterations exceeded):
    recompute all quantities in plan
    if delta < epsilon: break
  else:
    record DiagnosticKind::non_convergent_cycle for affected cycle

  compute_analytics(plan)
  return plan
```

**Rationale**: Backward expansion is natural for "I want X, what do I need?" planning.
Fixed-point is the standard approach for circular systems (input-output economic models,
material flow analysis). It converges for typical homestead graphs because resource
cycles are energy-dissipative (you always consume more than you internally produce for
at least one resource in the cycle, eventually requiring external input or reducing
quantities).

**Alternatives considered**:
- Forward simulation: requires specifying starting state, unnatural for planning.
- LP/MIP from the start: overkill for the initial version; ISolverStrategy interface
  preserves this path for a future feature without coupling it to the core algorithm.
- Symbolic cycle resolution: algebraic, exact, but very complex for seasonal graphs.

---

## 2. Fixed-Point Convergence Details

**Decision**: Gauss-Seidel style iteration (update in place, use new values immediately)
with a relative tolerance of 1e-6 and a hard cap of `SolverConfig::max_convergence_iterations`
(default 100).

**Convergence criterion**: `max over all quantities |new - old| / max(old, 1e-9) < 1e-6`

**Non-convergence detection**: If the cap is reached, record a `Diagnostic` with
`DiagnosticKind::non_convergent_cycle`, identify the resource cycle with the largest
residual, and use the last computed quantities as the plan output (best available).

**Rationale**: Gauss-Seidel converges faster than Jacobi for this class of problem.
The 1e-6 relative tolerance is tight enough for kg-scale agricultural quantities
(sub-microgram precision is irrelevant) while being achievable in < 10 iterations
for typical homestead cycles. 100 iterations is a safe upper bound for any realistic
graph; if it is hit, the cycle is genuinely infeasible or has a data error.

---

## 3. Loop Closure Score Formula

**Decision**:
```
loop_closure_score = Σ(internal_production[r] * weight[r]) /
                     Σ(total_demand[r] * weight[r])
```
where `weight[r]` is the economic value (cost_per_unit) of resource `r` from the
registry default costs, falling back to 1.0 if not set.

Score = 1.0 (100%) when all demand is met internally.
Score = 0.0 (0%) when all demand requires external purchase.

**Rationale**: Weighting by economic value gives a more meaningful score than a
simple resource-count ratio — 100% self-sufficiency in corn stover (low value)
while purchasing all protein feed (high cost) should not score as 50%.

**Alternatives considered**:
- Unweighted (count of resources satisfied internally / total): misleading for
  resources with vastly different economic significance.
- Caloric equivalence weighting: only valid for food resources, excludes materials.

---

## 4. Graph Representation

**Decision**: Adjacency list using `std::unordered_map<NodeId, std::vector<NodeId>>`
for successor and predecessor sets, plus a `std::unordered_map<NodeId, Node>` for node
data and `std::vector<ResourceFlow>` for edge data.

**NodeId**: `uint64_t`, monotonically increasing per-graph counter.

**Cycle detection**: Johnson's algorithm (finds all simple cycles) with DFS fallback
for existence-only queries. For typical homestead graphs (< 50 nodes), DFS is fast
enough; Johnson's is O((V+E)(C+1)) where C = number of cycles.

**Rationale**: Homestead graphs are sparse — a 200-entity plan has on the order of
300–400 edges, not O(n²). Adjacency lists are optimal for sparse graphs. A flat
`std::vector<ResourceFlow>` for edges allows O(n) scans and is cache-friendly.

**Alternatives considered**:
- Adjacency matrix: O(n²) memory, wasteful for sparse graphs.
- Boost.Graph: excluded by constitution (no Boost).
- std::graph (C++26 proposal): not yet available on GCC 13 / Clang 17.

---

## 5. Monthly Time-Step Simulation

**Decision**: The planning horizon is fixed at 12 months (calendar year). The solver
maintains a `MonthlyValues = std::array<double, 12>` for every resource quantity in
the plan. Each entity instance's cycle is mapped to the months it contributes output,
respecting `Lifecycle::active_months` (a 12-bit `MonthMask`).

**Cycle-to-month mapping**: A cycle of length `d` days starting in month `m` contributes
output in month `m + floor(d/30)` (clamped). For entities with `cycles_per_year > 1`
the contributions are distributed uniformly across active months.

**Rationale**: Monthly granularity is the right level for seasonal agricultural planning
— sub-monthly variation in yield averages out, and annual totals alone hide seasonal
imbalances (the main failure mode in closed-loop systems).

---

## 6. CMake FetchContent Patterns

**Decision**: Each dependency fetched with `FetchContent_Declare` + `FetchContent_MakeAvailable`.
Pin each to a specific git tag for reproducibility.

```cmake
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3)

FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.6.0)

FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.13.0)
```

`spdlog` is wrapped in `if(HOMESTEAD_ENABLE_LOGGING)` so it is not fetched for
default builds.

**Rationale**: Pinned tags give reproducible builds without a lockfile. FetchContent
avoids requiring system package managers. All three libraries support CMake
`find_package`-style consumption after `MakeAvailable`.

---

## 7. Property-Based Testing with Catch2 Generators

**Decision**: Use Catch2's `GENERATE` macro with custom generators for random entity
configurations. Key invariants to assert:

1. **Balance invariant**: For every resource `r` in a fully-solved plan with no
   Diagnostics, `internal_production[r] + external_purchase[r] >= consumption[r]`
   (monthly, for all 12 months).
2. **Loop score bounds**: `0.0 <= loop_closure_score <= 1.0`.
3. **Convergence**: For any plan that converged (no `non_convergent_cycle` Diagnostic),
   re-running the solver on the same inputs produces an identical `PlanResult`.
4. **Solver never throws**: For any input (including degenerate/empty goals), the solver
   returns without throwing an exception.

**Rationale**: Catch2 v3 generators are sufficient for this; a dedicated property-based
library (e.g., RapidCheck) would be better long-term but adds a dependency. The four
invariants above catch the most common regression classes.

---

## 8. spdlog Integration Strategy

**Decision**: Wrap spdlog behind a thin `homestead::log` facade in
`include/homestead/core/log.hpp` that compiles to no-ops when
`HOMESTEAD_ENABLE_LOGGING=OFF`. The facade uses a single `HOMESTEAD_LOG_DEBUG(...)` /
`HOMESTEAD_LOG_INFO(...)` / `HOMESTEAD_LOG_WARN(...)` macro set.

```cpp
#ifdef HOMESTEAD_ENABLE_LOGGING
  #include <spdlog/spdlog.h>
  #define HOMESTEAD_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
  #define HOMESTEAD_LOG_INFO(...)  spdlog::info(__VA_ARGS__)
  #define HOMESTEAD_LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#else
  #define HOMESTEAD_LOG_DEBUG(...) do {} while(false)
  #define HOMESTEAD_LOG_INFO(...)  do {} while(false)
  #define HOMESTEAD_LOG_WARN(...)  do {} while(false)
#endif
```

The macros live in `src/<module>/` files only — never in public headers — so library
consumers are never exposed to spdlog headers.

**Rationale**: Zero-overhead when disabled (macros → empty statements, optimized away).
Macros in private sources only ensures `homestead::core`'s no-external-dependency
contract is not violated.

---

## 9. default_registry.json Entity Coverage

**Decision**: Ship 20 entity templates covering the Sisteminha modules, with values
sourced from Embrapa technical documents:

| Entity slug | Module | Cycle | Key outputs |
|---|---|---|---|
| `broiler_chicken` | Poultry | 60 d | meat, manure, feathers |
| `laying_hen` | Poultry | continuous | eggs (300/yr), manure |
| `quail` | Poultry | continuous | eggs (280/yr), manure |
| `tilapia_tank_5000l` | Aquaculture | 120 d | tilapia_whole_kg, nutrient_water_l |
| `compost_bin` | Soil | 90 d | mature_compost |
| `earthworm_bin` | Soil | continuous | vermicompost, earthworms |
| `biodigester` | Energy | continuous | biogas, biofertilizer |
| `lettuce_bed_1m2` | Vegetables | 45 d | lettuce_head |
| `tomato_bed_1m2` | Vegetables | 90 d | tomato_kg |
| `pepper_bed_1m2` | Vegetables | 75 d | pepper_kg |
| `corn_plot_1m2` | Grains | 120 d | corn_grain_kg, corn_stover |
| `bean_plot_1m2` | Legumes | 90 d | bean_kg, straw |
| `cassava_plot_1m2` | Roots | 365 d | cassava_kg, leaves |
| `banana_plant` | Fruit | 365 d | banana_bunch_kg |
| `papaya_plant` | Fruit | continuous | papaya_kg |
| `acerola_plant` | Fruit | continuous | acerola_kg |
| `goat` | Livestock | continuous | milk_l, manure, kids |
| `goat_kids` | Livestock | 150 d | goat_meat_kg |
| `water_tank` | Infrastructure | — | stored_water_l |
| `rainwater_collector` | Infrastructure | — | stored_water_l |

Resources covered (≥ 60 slugs): the full set of inputs and outputs for each entity
above, including feed types, water, labor-hours, currencies, soil nutrients (N, P, K
as explicit resources), and construction materials.

**Rationale**: This set covers all six Sisteminha "rings" (poultry, aquaculture, soil
management, horticulture, energy, fruit/perennials) and includes infrastructure
entities that the solver can instantiate to close water loops. 20 entities is
sufficient for meaningful integration tests with circular dependencies.
