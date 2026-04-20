# Data Model: Composition-Aware Resource Matching (004)

## Modified: Entity (include/homestead/core/entity.hpp)

Two new plain-data fields added to the `Entity` struct. Both default to empty
(no-composition entity behaves identically to current code).

```cpp
/// Chemical or nutritional amounts this entity requires per base unit per cycle,
/// expressed as grams per unit (or kcal per unit for energy_kcal).
/// Keys match Resource::composition keys:
///   "protein_g"   — crude protein in grams
///   "energy_kcal" — metabolisable energy in kcal  (excluded from loop closure score)
///   "N_g"         — nitrogen in grams
///   "P_g"         — phosphorus in grams
///   "K_g"         — potassium in grams
/// Empty map → pure slug-based matching (no composition pass involvement).
/// Used by: animal entities and per-plant crop entities.
std::unordered_map<std::string, double> composition_requirements;

/// Area-scaled fertilisation requirement for area-based crop entities.
/// Grams of each element needed per m² of this entity per cycle.
/// Keys: "N_g", "P_g", "K_g".
/// Solver scales at runtime: demand = value × area_m2 × quantity_instances.
/// Empty map → entity not involved in Pass 2.
/// Used by: lettuce_bed_1m2, tomato_bed_1m2, pepper_bed_1m2, corn_plot_1m2,
///          cassava_plot_1m2 (area-based crop entities only).
std::unordered_map<std::string, double> fertilization_per_m2;
```

**Validation rules** (registry.cpp):
- All keys must be non-empty strings.
- All values must be > 0.0.
- Empty maps are valid (means "no composition requirement").
- Violation returns `RegistryError{RegistryErrorKind::invalid_quantity, ...}`.

---

## Modified: ResourceBalance (include/homestead/solver/result.hpp)

One new field added. Existing fields are unchanged.

```cpp
/// Net grams of each chemical element routed through this resource to meet
/// composition-matched demands across the plan.
/// Keys are element keys (N_g, P_g, K_g, protein_g, organic_matter_g).
/// energy_kcal is NOT stored here (excluded from loop closure score).
/// Empty when this resource was never composition-matched.
std::unordered_map<std::string, double> composition_routed;
```

---

## Modified: DiagnosticKind (include/homestead/solver/result.hpp)

One new enumerator:

```cpp
composition_gap  ///< Elemental requirement partially or fully purchased externally
```

Diagnostic message format:
```
"{Element} gap: {shortfall:.1f} g purchased externally for {entity_slug}"
```
Example: `"Nitrogen gap: 3.2 g purchased externally for corn_plot_1m2"`

---

## Registry migration — full entity table

### Animal entities → composition_requirements

| Entity | Remove input | Add composition_requirements |
|---|---|---|
| broiler_chicken | poultry_feed_kg | `{"protein_g": 720.0, "energy_kcal": 11600.0}` |
| laying_hen | poultry_feed_kg | `{"protein_g": 21.6, "energy_kcal": 348.0}` |
| quail | poultry_feed_kg | `{"protein_g": 5.04, "energy_kcal": 81.2}` |
| goat | goat_feed_kg | `{"protein_g": 180.0, "energy_kcal": 2700.0}` |
| goat_kids | goat_feed_kg | `{"protein_g": 60.0, "energy_kcal": 900.0}` |

### Area-based crop entities → fertilization_per_m2

| Entity | Remove input | Add fertilization_per_m2 |
|---|---|---|
| lettuce_bed_1m2 | mature_compost_kg | `{"N_g": 10.0, "P_g": 4.0, "K_g": 8.0}` |
| tomato_bed_1m2 | mature_compost_kg | `{"N_g": 15.0, "P_g": 6.0, "K_g": 12.0}` |
| pepper_bed_1m2 | mature_compost_kg | `{"N_g": 12.0, "P_g": 5.0, "K_g": 10.0}` |
| corn_plot_1m2 | mature_compost_kg | `{"N_g": 8.0, "P_g": 3.0, "K_g": 6.0}` |
| cassava_plot_1m2 | mature_compost_kg | `{"N_g": 5.0, "P_g": 2.0, "K_g": 8.0}` |

### Per-plant crop entities → composition_requirements (N/P/K per plant per cycle)

Banana, papaya, acerola are per-plant (not area-scaled), so they use
`composition_requirements` with N/P/K, not `fertilization_per_m2`.

| Entity | Remove input | Add composition_requirements |
|---|---|---|
| banana_plant | mature_compost_kg | `{"N_g": 120.0, "P_g": 40.0, "K_g": 200.0}` |
| papaya_plant | mature_compost_kg | `{"N_g": 5.0, "P_g": 2.0, "K_g": 7.0}` |
| acerola_plant | mature_compost_kg | `{"N_g": 3.3, "P_g": 1.5, "K_g": 4.0}` |

### Unchanged entities (rationale)

| Entity | Input kept | Reason |
|---|---|---|
| tilapia_tank_5000l | fish_feed_kg | No internal producer exists; slug stays to avoid diagnostic noise |
| tilapia_tank_5000l | tilapia_fingerlings | Biological input, non-substitutable |
| biodigester | chicken_manure_kg | Process-specific (not a fertilizer substitute in this context) |
| bean_plot_1m2 | (none to remove) | Already has no compost input; N-fixing legume |
| All entities | fresh_water_l | Volume requirement, not composition |
| All entities | human_labor_hours | Non-substitutable |
| All entities | construction materials | InfrastructureSpec slug inputs |

---

## Feed resource composition map additions

Keys added to `Resource::composition` for solver-time matching.
Values: grams of element per 1 unit of the resource (1 kg for kg-unit resources).

| Resource | protein_g | energy_kcal |
|---|---|---|
| corn_grain_kg | 93.0 | 3580.0 |
| bean_kg | 220.0 | 3400.0 |
| cassava_kg | 14.0 | 1590.0 |
| cassava_leaves_kg | 200.0 | 900.0 |

These mirror values already present in each resource's `NutritionalProfile`
(protein_g_per_kg × 1 kg = protein_g; calories_kcal_per_kg × 1 kg = energy_kcal).
Both representations coexist: `nutrition` for human food analysis, `composition` for
solver feed matching.

---

## Plan schema version

`PLAN_SCHEMA_VERSION` bumped: **1.1.0 → 1.2.0**

Backward-compatibility rule for deserialization: if `composition_routed` is absent
in a ResourceBalance JSON object (1.0.x or 1.1.x file), emit an empty map `{}`.
No error. This is an additive change.
