# Research: Nutrient and Soil Balance Tracking (N-P-K)

**Feature**: 003-npk-soil-balance | **Date**: 2026-04-09

## §1 — Registry composition key format

**Decision**: Keep existing keys `N_percent`, `P_percent`, `K_percent` (solids)
and `N_ppm`, `P_ppm` (liquids). Do not rename to `N_pct`.

**Rationale**: The keys are already populated in `default_registry.json` and
renaming them would break any downstream consumers serializing/deserializing the
registry. The analytics layer detects unit type by checking which key suffix is
present, which is zero-cost at runtime.

**Alternatives considered**: Standardizing to `N_pct` across the board —
rejected because it requires migrating existing data and touches the serialization
contract without adding correctness.

---

## §2 — Unit system: grams throughout

**Decision**: All NutrientDemand fields and NutrientBalance arrays store values
in **grams** (g).

**Rationale**: The registry mixes kg-based solids and liter-based liquids with
ppm units. A single gram-level canonical unit allows both to coexist without
lossy floating-point conversion at load time. Values in the lettuce-scale range
(0.010 kg = 10 g) are more legible as integers or small decimals in grams.

Unit conversions at computation time only:
- Solid `N_percent`: `g = output_kg × (percent/100) × 1000`
- Liquid `N_ppm`: `g = output_liters × (ppm / 1000)`

**Alternatives considered**: Storing kg throughout — rejected because the ppm
→ kg conversion (`ppm/1000/1000`) produces four-decimal values that are harder
to validate in tests. Grams keep ppm conversion to a single division.

---

## §3 — Supply from net output (not gross)

**Decision**: Available N/P/K for a fertilizer resource is computed from:
`net = gross_internal_production − consumption_by_non_crop_entities`
(clamped to 0.0).

**Rationale**: When a fertilizer resource flows through multiple transformation
stages (e.g., compost → earthworm bin → vermicompost), counting gross production
at each stage double-counts the underlying nutrients. Subtracting only the
consumption by non-crop intermediate entities (those without `nutrient_demand`)
leaves the supply that actually reaches crops, plus any unconsumed surplus.

Crop entity consumption of a fertilizer resource is NOT subtracted — that
consumption is the fertilizer application event, and its nutrient content is
already reflected in `available_n/p/k`.

**Alternatives considered**:
- Gross production only — rejected (double-counts transformed nutrients).
- Supply from crop inputs only — rejected (omits surplus fertilizer that is
  available but unclaimed by current crop entities).

---

## §4 — `cycles_in_month()` satisfies prorated Q1 requirement

**Decision**: Re-use the existing `cycles_in_month(lifecycle, month)` function
for both supply and demand computation. No new helper needed.

**Rationale**: The function already returns `cycles_per_year / active_month_count`
for active months, distributing production uniformly (proportionally) across all
months in which the entity is active. A 6-cycle/year crop active in 6 months
gets 1.0 cycles/month — exactly the proportional prorated distribution Q1
requires. An entity active in all 12 months gets `cycles_per_year / 12` per
month.

**What Q1 prorating means in this model**: The Lifecycle struct does not record
a specific start day; it records `cycle_days` and `cycles_per_year`. The
proportional distribution is the highest-fidelity prorating available without
introducing per-instance start dates.

**Alternatives considered**: A day-level proration function that tracks exact
cycle overlap with each calendar month — rejected because Lifecycle has no
cycle-start date, so this would require a new data field (out of scope).

---

## §5 — `std::optional<NutrientBalance>` semantics

**Decision**: `PlanResult::nutrient_balance` is `std::optional<NutrientBalance>`.
- `nullopt` → plan has no area-based crop entity instances (nothing to balance).
- Zero-initialized `NutrientBalance{}` → deserialized from a 1.0.x plan file
  (Q2 backward-compat rule).
- Non-trivial value → normal post-solve result.

**Rationale**: An optional communicates clearly that the concept of nutrient
balance is inapplicable for plans without crops, rather than returning arrays
of zeros that could be misread as "supply and demand are both zero." The zero-
init path for old files prevents deserialization errors without requiring user
action.

**Alternatives considered**: Always-present (non-optional) NutrientBalance —
rejected because zero arrays for a fish-only plan are semantically meaningless
and could mask bugs.

---

## §6 — `NutrientDemand` in `homestead::core`, not `homestead::solver`

**Decision**: `NutrientDemand` is a field on `Entity` in `homestead::core`.

**Rationale**: `Entity` is a registry domain type. Its nutrient demand is a
property of the entity type (e.g., lettuce always needs 10 g N/m²/cycle),
not a solver computation. Putting it in `core` keeps the domain model complete
and allows the serialization module to round-trip entities independently of the
solver.

**Constraint respected**: `homestead::core` has no external dependencies. The
`NutrientDemand` struct uses only `double` — no stdlib containers, no headers
beyond `<optional>` on the `Entity` side.

---

## §7 — Per-plant entities excluded from this feature

**Decision**: `banana_plant`, `papaya_plant`, `acerola_plant` do NOT receive
`nutrient_demand` data in this feature.

**Rationale**: `NutrientDemand` fields are `_per_m2_per_cycle`. Per-plant
entities have quantity expressed as a plant count, not an area. Applying
`area_m2 × demand_per_m2` to a per-plant entity produces meaningless results.
A future feature can introduce a `NutrientDemand` variant with `_per_unit`
fields for non-area entities.

---

## §8 — Integration test: Sisteminha expected values (pre-calculation)

The integration test (Test 2, fixed predetermined outcome) requires hand-
calculated reference values. Using registry quantities:

**Scenario**: 1× corn plot (1 m², `corn_plot_1m2`) + 1× chicken coop (minimal).

Corn demand (active all 12 months, 4 cycles/year):
- `cycles_in_month` = 4/12 = 0.333 cycles/month
- N demand per month = 8.0 g/m²/cycle × 1 m² × 1 instance × 0.333 = 2.667 g N/month

Chicken coop → mature compost via compost bin (simplified path):
- chicken_manure_kg N_percent = 1.5%; small coop produces ~2 kg manure/month
- mature_compost_kg N_percent = 1.2%; conversion rate ~0.5 kg compost per kg manure
- net compost ≈ 1.0 kg/month → 1.0 × 1.2/100 × 1000 = 12 g N/month

In this scenario: available_N (12 g) > demanded_N (2.667 g) → NO deficit.
To trigger a deficit test: scale corn to 10 m² → demanded = 26.67 g/month,
available ≈ 12 g → deficit = 14.67 g/month.

These values are approximate and must be recomputed precisely from the actual
entity quantities in the registry when writing the test. The test must hard-code
the exact computed expected values (to 1e-6 tolerance), not accept a range.
