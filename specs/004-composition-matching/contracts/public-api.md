# Public API Contract: Composition-Aware Resource Matching (004)

This document specifies all public API additions and changes. The library's
existing API surface is unchanged; all additions are strictly additive.

---

## homestead::Entity (include/homestead/core/entity.hpp)

### New field: composition_requirements

```cpp
std::unordered_map<std::string, double> composition_requirements;
```

- **Default**: empty map (no composition requirements)
- **Keys**: any non-empty string matching a key in `Resource::composition`
  (e.g., `"protein_g"`, `"energy_kcal"`, `"N_g"`, `"P_g"`, `"K_g"`)
- **Values**: amount of that element needed per individual entity instance per cycle,
  in grams (or kcal for `energy_kcal`)
- **Invariant**: all values > 0.0 when non-empty; enforced by Registry validation
- **Serialized as**: JSON object `{"protein_g": 720.0, "energy_kcal": 11600.0}`
  under key `"composition_requirements"` on the entity JSON object

### New field: fertilization_per_m2

```cpp
std::unordered_map<std::string, double> fertilization_per_m2;
```

- **Default**: empty map
- **Keys**: `"N_g"`, `"P_g"`, `"K_g"` only (other keys are silently ignored by
  the solver's Pass 2)
- **Values**: grams of that element needed per m² of this entity per cycle
- **Scaling**: solver computes total demand as `value * area_m2 * quantity_instances`
- **Invariant**: all values > 0.0 when non-empty; enforced by Registry validation
- **Serialized as**: JSON object under key `"fertilization_per_m2"`

---

## homestead::ResourceBalance (include/homestead/solver/result.hpp)

### New field: composition_routed

```cpp
std::unordered_map<std::string, double> composition_routed;
```

- **Default**: empty map (resource was not involved in any composition-matching)
- **Keys**: element keys (e.g., `"N_g"`, `"P_g"`, `"K_g"`, `"protein_g"`)
  excluding `energy_kcal`
- **Values**: total grams of that element routed through this resource to meet
  composition demands across the entire plan (annual total, not monthly)
- **Invariant**: all values ≥ 0.0; map is empty when no composition routing occurred
- **Serialized as**: JSON object under key `"composition_routed"` on the
  resource balance JSON object; absent in 1.1.x files → deserialize as empty map

---

## homestead::DiagnosticKind (include/homestead/solver/result.hpp)

### New enumerator: composition_gap

```cpp
composition_gap  ///< Elemental requirement partially or fully purchased externally
```

- **Emitted when**: any element in `composition_requirements` or any element derived
  from `fertilization_per_m2` has remaining demand after all internal candidates are
  exhausted
- **Message format**: `"{Element} gap: {shortfall:.1f} g purchased externally for {entity_slug}"`
- **resource_slug field**: the synthetic external slug (e.g., `"N_g_external"`)
- **entity_slug field**: the entity with unmet demand
- **to_string(DiagnosticKind::composition_gap)** returns `"composition_gap"`

---

## homestead::Registry validation (src/core/registry.cpp)

### register_entity — new validation rules

Added to the existing `register_entity` validation pass:

1. For each entry in `entity.composition_requirements`:
   - Key must be a non-empty string
   - Value must be > 0.0
   - Violation: `RegistryError{RegistryErrorKind::invalid_quantity, "composition_requirements key '<k>': value must be > 0"}`

2. For each entry in `entity.fertilization_per_m2`:
   - Key must be a non-empty string
   - Value must be > 0.0
   - Violation: `RegistryError{RegistryErrorKind::invalid_quantity, "fertilization_per_m2 key '<k>': value must be > 0"}`

All other existing validation rules are unchanged.

---

## Solver behaviour contract

### Pass 1: feed matching (composition_requirements)

Runs after the slug-based demand queue drains and before `populate_plan_result()`.

- **Inputs**: graph with all entity instances already present
- **For each EntityInstanceNode** with `entity.composition_requirements` non-empty:
  1. Compute scaled demand per element:
     `demand[key] = requirements[key] * quantity_instances * cycles_in_month(lc, rep_month)`
  2. Collect candidate resources: all resources produced by EntityInstanceNodes
     already in the graph whose `Resource::composition` map contains the required keys
  3. Sort candidates by loop-closure contribution (descending) —
     proxy: number of outputs (same heuristic as `select_entity_slug`)
  4. Determine primary allocation key from the entity's requirement key set:
     - If `energy_kcal` is present → primary = `energy_kcal`, secondary = `protein_g`
     - If `energy_kcal` is absent but `N_g` is present → primary = `N_g`; treat
       `P_g` and `K_g` as side-benefit keys (same pattern as Pass 2)
     - If neither `energy_kcal` nor `N_g` is present → primary = first key in
       the requirement map sorted alphabetically
     This rule handles per-plant crop entities (banana_plant, papaya_plant,
     acerola_plant) which declare N_g/P_g/K_g in composition_requirements but
     no energy_kcal.
  5. For each candidate (sorted): compute how many units of that resource cover
     remaining primary-key demand; check if that quantity also covers remaining
     secondary/side-benefit key demand; allocate. Credit all keys simultaneously
     from the same unit allocation.
  6. Unmet remainder → ExternalSourceNode with synthetic slug `"{key}_external"`
     (e.g., `"energy_kcal_external"`, `"protein_g_external"`)
  7. Emit `DiagnosticKind::composition_gap` for each unmet elemental remainder

### Pass 2: fertilization matching (fertilization_per_m2)

- **Inputs**: same as Pass 1 (graph post slug-expansion)
- **For each EntityInstanceNode** with `entity.fertilization_per_m2` non-empty:
  1. Compute scaled demand: `demand[key] = per_m2[key] * area_m2 * qty * cpm`
  2. Collect candidate resources: EntityInstanceNode outputs with N_g, P_g, or K_g
     in their `Resource::composition`
  3. Sort by loop-closure contribution
  4. Primary key: `N_g`; check P_g and K_g coverage from the same allocation
  5. For each candidate: allocate to meet N demand; credit P and K as side benefit
  6. Unmet per-element remainder → ExternalSourceNode `"N_g_external"` etc.
  7. Emit `DiagnosticKind::composition_gap` per unmet element

### Post-pass Gauss-Seidel iteration

After both passes, one additional Gauss-Seidel iteration runs (same convergence
condition: quantities stable and `unsatisfied_inputs()` empty).

### Isolation guarantee

Neither pass calls `add_entity_instance()`. Entity topology and quantities are
unchanged by composition matching. This is a non-negotiable contract.

---

## Serialization contract (src/serialization/serialization.cpp)

### Entity serialization (entity_to_json / entity_from_json)

```json
{
  "slug": "broiler_chicken",
  "composition_requirements": {"protein_g": 720.0, "energy_kcal": 11600.0},
  "fertilization_per_m2": {}
}
```

- Absent `composition_requirements` key in JSON → deserialize as empty map
- Absent `fertilization_per_m2` key in JSON → deserialize as empty map
- Both serialize as JSON objects (not arrays)

### ResourceBalance serialization

```json
{
  "resource_slug": "corn_grain_kg",
  "composition_routed": {"protein_g": 150.0, "N_g": 0.0}
}
```

- Absent `composition_routed` in older files → deserialize as empty map
- Zero-valued entries MAY be omitted during serialization for compactness

### Plan schema version

Files written by this feature version carry `"version": "1.2.0"` in the plan JSON.
Files with version 1.0.x or 1.1.x deserialize `composition_routed` as empty map.
