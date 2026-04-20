# Quickstart Scenarios: Composition-Aware Resource Matching (004)

These scenarios map directly to the five integration tests in
`tests/solver/test_composition_matching.cpp`.

---

## Scenario 1 — Broiler protein met from corn + bean (US1 / P1)

**Setup**: Registry has broiler_chicken (composition_requirements: protein_g=720,
energy_kcal=11600 per cycle), corn_plot_1m2 (produces corn_grain_kg: protein_g=93,
energy_kcal=3580 per kg), and bean_plot_1m2 (produces bean_kg: protein_g=220,
energy_kcal=3400 per kg).

**Goals**: `broiler_meat_kg=5.0/month` (→ ~5 broiler instances), plus enough grain
to provide more than 720 g protein per cycle.

**Expected solver output**:
- Solver instantiates corn_plot and bean_plot from goals.
- Pass 1: routes corn_grain_kg and/or bean_kg to broiler instances.
- `PlanResult.gap_report` contains NO entry for `"protein_g_external"`.
- `PlanResult.diagnostics` contains NO `composition_gap` entries.
- `ResourceBalance` for `corn_grain_kg` has non-empty `composition_routed["protein_g"]`.

**Key assertion**:
```cpp
bool found_protein_gap = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
    return d.kind == DiagnosticKind::composition_gap &&
           d.resource_slug == "protein_g_external";
});
REQUIRE_FALSE(found_protein_gap);
```

---

## Scenario 2 — Corn N/P/K met from chicken manure (US2 / P2)

**Setup**: corn_plot_1m2 has fertilization_per_m2: {N_g: 8.0, P_g: 3.0, K_g: 6.0}.
Registry has chicken_manure_kg with composition {N_g: 15.0, P_g: 12.0, K_g: 8.0}
(post-Feature 008 values).

**Goals**: `broiler_meat_kg=5.0/month` (→ chicken manure in graph) +
`corn_grain_kg=3.0/month` (→ corn_plot in graph).

**Expected**:
- Pass 2 routes chicken_manure_kg to corn_plot's N/P/K requirement.
- NO `composition_gap` diagnostic for N, P, or K for `corn_plot_1m2`.
- `ResourceBalance` for `chicken_manure_kg`.`composition_routed["N_g"]` > 0.

**Key assertion**:
```cpp
for (const auto& d : result.diagnostics) {
    if (d.kind == DiagnosticKind::composition_gap &&
        d.entity_slug == "corn_plot_1m2") {
        FAIL("Unexpected composition_gap for corn_plot_1m2: " << d.message);
    }
}
```

---

## Scenario 3 — Partial fertilization via nutrient_water_l (US2 / US3)

**Setup**: Goals include corn_plot (needs N, P, K) and tilapia_tank_5000l
(produces nutrient_water_l with {N_g: 0.036, P_g: 0.0096} per litre — post-Feature
008 values). No manure-producing entity in the plan.

**Goals**: `tilapia_whole_kg=1.0/month` + `corn_grain_kg=1.0/month`.

**Hand-calculated K shortfall**: `nutrient_water_l` has no `K_g` key in its
composition, so the full K demand is purchased externally. K shortfall (g) =
`fertilization_per_m2["K_g"]` × `area_m2` × `corn_qty` × `cpm`, where:
- `fertilization_per_m2["K_g"]` = 6.0 g/m²/cycle (from data-model.md)
- `area_m2` = 1.0 (corn_plot_1m2 entity definition)
- `corn_qty` = `ceil(1.0 / corn_grain_kg_per_cycle)` (from solver result or fixture)
- `cpm` = `corn_cycle_days / 30.0` (from entity definition)
Record `corn_cycle_days` from the entity in the test fixture so the assertion
is derived from the same values driving the solver.

**Expected**:
- Pass 2 routes nutrient_water_l to corn_plot, covering some N and P but zero K.
- `composition_gap` diagnostic is emitted for K (and possibly for N/P if water
  supply is insufficient).
- Gap report contains `"K_g_external"` entry with `shortfall_g` matching the
  hand-calculated value above.
- `ResourceBalance` for `nutrient_water_l`.`composition_routed["N_g"]` > 0.

**Key assertion**:
```cpp
// Hand-calculated: nutrient_water_l has no K_g → full K demand is external.
const double k_per_m2 = 6.0;      // fertilization_per_m2["K_g"] from fixture
const double area_m2  = 1.0;      // corn_plot_1m2 area
const double corn_qty = result.entity_quantities.at("corn_plot_1m2");
const double cpm      = static_cast<double>(corn_cycle_days) / 30.0;
const double expected_k_gap_g = k_per_m2 * area_m2 * corn_qty * cpm;

auto k_diag = std::ranges::find_if(result.diagnostics, [](const Diagnostic& d) {
    return d.kind == DiagnosticKind::composition_gap &&
           d.resource_slug == "K_g_external";
});
REQUIRE(k_diag != result.diagnostics.end());
REQUIRE(k_diag->shortfall_g == Approx(expected_k_gap_g).margin(0.01));
```

---

## Scenario 4 — Regression: no composition requirements (US4 / P4)

**Setup**: Registry loaded from defaults, but temporarily strip composition_requirements
and fertilization_per_m2 from all entities (or use a custom entity set with empty maps).

**Goals**: Same goals as an existing test that was passing before Feature 004.

**Expected**:
- `PlanResult` is bit-for-bit identical to the result from the same goals + same
  registry WITHOUT the composition matching passes.
- No `composition_gap` diagnostics.
- `composition_routed` is empty on all ResourceBalance entries.

**Key assertion**:
```cpp
REQUIRE(result_with_composition.loop_closure_score ==
        Approx(result_baseline.loop_closure_score));
REQUIRE(result_with_composition.graph.node_count() ==
        result_baseline.graph.node_count());
```

---

## Scenario 5 — nutrient_water_l routes without explicit slug link (US3 / P3)

**Setup**: tilapia_tank_5000l produces `nutrient_water_l`. One crop entity
(e.g., lettuce_bed_1m2) has `fertilization_per_m2: {N_g: 10.0, ...}`.
Neither the tilapia entity nor the lettuce entity declares the other.

**Goals**: `tilapia_whole_kg=1.0/month` + `lettuce_head=10.0/month`.

**Expected**:
- Pass 2 discovers that `nutrient_water_l` contains N_g and P_g (from its
  `Resource::composition` map, populated by Feature 008).
- Routes `nutrient_water_l` to `lettuce_bed_1m2`'s N/P demand without any
  explicit slug link declared in either entity.
- A `ResourceFlow` edge exists in the graph from the tilapia entity node
  to the lettuce entity node carrying resource `nutrient_water_l`.

**Key assertion**:
```cpp
bool found_flow = false;
result.graph.for_each_edge([&](const ResourceFlow& f) {
    if (f.resource_slug == "nutrient_water_l") {
        found_flow = true;
    }
});
REQUIRE(found_flow);
```
