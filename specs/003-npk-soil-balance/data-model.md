# Data Model: Nutrient and Soil Balance Tracking (N-P-K)

**Feature**: 003-npk-soil-balance | **Date**: 2026-04-09

## New Types

### `NutrientDemand` (`homestead::core`)

Owned by: `include/homestead/core/nutrient.hpp`

| Field | Type | Unit | Description |
|---|---|---|---|
| `n_g_per_m2_per_cycle` | `double` | g N / m² / cycle | Nitrogen demand per area per growing cycle |
| `p_g_per_m2_per_cycle` | `double` | g P / m² / cycle | Phosphorus demand |
| `k_g_per_m2_per_cycle` | `double` | g K / m² / cycle | Potassium demand |

**Validation rules**:
- All fields default to `0.0` (zero-value default)
- Negative values are not meaningful but not validated at load time (treated as 0)
- Present only on area-based crop entities; `std::nullopt` on all others

**JSON representation** (in entity object):
```json
"nutrient_demand": {
    "n_g_per_m2_per_cycle": 10.0,
    "p_g_per_m2_per_cycle": 4.0,
    "k_g_per_m2_per_cycle": 8.0,
    "_source": "Embrapa Hortaliças, Circular Técnica 40"
}
```
`_source` is a metadata string, ignored by the deserializer, preserved in the
JSON file as documentation.

---

### `NutrientBalance` (`homestead::solver`)

Owned by: `include/homestead/solver/result.hpp`

| Field | Type | Unit | Description |
|---|---|---|---|
| `available_n` | `MonthlyValues` | g N / month | Net nitrogen supplied by fertilizer-producing entities |
| `available_p` | `MonthlyValues` | g P / month | Net phosphorus supplied |
| `available_k` | `MonthlyValues` | g K / month | Net potassium supplied |
| `demanded_n` | `MonthlyValues` | g N / month | Nitrogen demanded by all crop entity instances |
| `demanded_p` | `MonthlyValues` | g P / month | Phosphorus demanded |
| `demanded_k` | `MonthlyValues` | g K / month | Potassium demanded |

`MonthlyValues` = `std::array<double, 12>`. Index 0 = January, index 11 = December.

**Relationships**:
- Produced by `compute_nutrient_balance()` in `analytics.cpp`
- Stored as `std::optional<NutrientBalance>` on `PlanResult`
- Read by `check_nutrient_deficits()` in `convergence.cpp`

**JSON representation** (in plan result object):
```json
"nutrient_balance": {
    "available_n": [12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0, 12.0],
    "available_p": [...],
    "available_k": [...],
    "demanded_n":  [...],
    "demanded_p":  [...],
    "demanded_k":  [...]
}
```
Field omitted entirely when `std::nullopt` (no crop entities in the plan).

---

## Modified Types

### `Entity` — new field

```
Entity::nutrient_demand : std::optional<NutrientDemand>
```

- `std::nullopt` (default) → entity excluded from nutrient demand calculation
- Set on: `lettuce_bed_1m2`, `tomato_bed_1m2`, `pepper_bed_1m2`,
  `corn_plot_1m2`, `bean_plot_1m2`, `cassava_plot_1m2`
- NOT set on: animals, composters, fish tanks, fruit trees

### `DiagnosticKind` — new enum value

```
DiagnosticKind::nutrient_deficit
```

Serialized as the string `"nutrient_deficit"` in JSON diagnostic arrays.

Diagnostic message format:
```
"{Element} deficit in {Mon}: {available:.1f} g available vs {demanded:.1f} g demanded (shortfall: {shortfall:.1f} g)"
```
Example: `"Nitrogen deficit in Mar: 12.0 g available vs 26.7 g demanded (shortfall: 14.7 g)"`

### `PlanResult` — new field

```
PlanResult::nutrient_balance : std::optional<NutrientBalance>
```

`nullopt` iff the solved graph contains no entity instances with `nutrient_demand` set.

---

## Resource Composition Map — Existing Keys (unchanged)

The `Resource::composition` map (type: `std::unordered_map<std::string, double>`)
uses these keys for N-P-K tracking:

| Key | Unit | Applies to | Example |
|---|---|---|---|
| `N_percent` | % dry weight | Solid resources (kg-based) | `chicken_manure_kg`: 1.5 |
| `P_percent` | % dry weight | Solid resources | `chicken_manure_kg`: 1.2 |
| `K_percent` | % dry weight | Solid resources | `chicken_manure_kg`: 0.8 |
| `N_ppm` | mg/L | Liquid resources (L-based) | `nutrient_water_l`: 30.0 |
| `P_ppm` | mg/L | Liquid resources | `nutrient_water_l`: 8.0 |

Missing keys default to 0.0 contribution (Q3 — no error, no warning).

---

## Schema Version

| Version | Change |
|---|---|
| `1.0.0` | Baseline (pre-feature) |
| `1.1.0` | Added `Entity::nutrient_demand`, `PlanResult::nutrient_balance`, `DiagnosticKind::nutrient_deficit` |

**Backward compatibility**: Plans with version < `1.1.0` deserialize with
`nutrient_balance = NutrientBalance{}` (all zeros). Entities without
`nutrient_demand` field deserialize with `nutrient_demand = std::nullopt`.
Both are lossless — no data in old files is discarded.

---

## Compute Flow

```
populate_plan_result()
  ├── compute_balance_sheet()     → PlanResult::balance_sheet  [existing]
  ├── compute_labor_schedule()    → PlanResult::labor_schedule [existing]
  ├── compute_bom()               → PlanResult::bom            [existing]
  ├── compute_loop_closure_score()→ PlanResult::loop_closure_score [existing]
  ├── check_seasonality()         → PlanResult::diagnostics    [existing]
  ├── check_labor_constraint()    → PlanResult::diagnostics    [existing]
  ├── compute_nutrient_balance()  → PlanResult::nutrient_balance [NEW]
  └── check_nutrient_deficits()   → PlanResult::diagnostics    [NEW]
```

`compute_nutrient_balance()` reads `PlanResult::balance_sheet` (already
populated) plus iterates the graph for entity instance quantities. No
solver state is modified; this is a pure read → compute → write flow.
