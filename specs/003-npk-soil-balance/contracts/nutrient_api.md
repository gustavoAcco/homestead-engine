# Public API Contract: Nutrient Balance (003-npk-soil-balance)

**Target**: `homestead::core` and `homestead::solver` library consumers  
**Schema version**: 1.1.0

## New public surface

### `homestead/core/nutrient.hpp`

```cpp
namespace homestead {

struct NutrientDemand {
    double n_g_per_m2_per_cycle{};
    double p_g_per_m2_per_cycle{};
    double k_g_per_m2_per_cycle{};
};

}
```

**Stability**: Stable. Field additions are minor-version changes.

---

### `homestead/core/entity.hpp` — addition to `Entity`

```cpp
std::optional<NutrientDemand> nutrient_demand;  // nullopt for non-crop entities
```

**Access pattern**:
```cpp
if (entity.nutrient_demand) {
    double n = entity.nutrient_demand->n_g_per_m2_per_cycle;
}
```

---

### `homestead/solver/result.hpp` — additions

#### `NutrientBalance`

```cpp
struct NutrientBalance {
    MonthlyValues available_n{};  // g N supplied per month (net output)
    MonthlyValues available_p{};
    MonthlyValues available_k{};
    MonthlyValues demanded_n{};   // g N demanded by crops per month
    MonthlyValues demanded_p{};
    MonthlyValues demanded_k{};
};
```

**Access pattern**:
```cpp
if (result.nutrient_balance) {
    const auto& nb = *result.nutrient_balance;
    for (int m = 0; m < 12; ++m) {
        double deficit_n = nb.demanded_n[m] - nb.available_n[m];
        // deficit_n > 0 → nitrogen shortfall this month
    }
}
// nullopt → plan has no area-based crop entity instances
```

#### `DiagnosticKind::nutrient_deficit`

Emitted when `demanded > available` for any element in any month.  
Message format: `"{Element} deficit in {Mon}: {avail:.1f} g available vs {demand:.1f} g demanded (shortfall: {shortfall:.1f} g)"`

**Filter pattern**:
```cpp
for (const auto& d : result.diagnostics) {
    if (d.kind == DiagnosticKind::nutrient_deficit) {
        // handle nutrient deficit
    }
}
```

---

## Breaking changes

None. All additions are opt-in (optional field on Entity, optional field on
PlanResult). Existing code that does not read `nutrient_demand` or
`nutrient_balance` compiles and runs identically.

## Serialization contract

Plans serialized with schema `1.0.0` deserialize correctly under this feature:
- `PlanResult::nutrient_balance` → zero-initialized `NutrientBalance{}` (not nullopt)
- `Entity::nutrient_demand` → `std::nullopt` (field absent in old JSON)

Plans serialized with schema `1.1.0` are not readable by `1.0.0` consumers
without upgrade — the new fields are simply ignored by consumers that use
`nlohmann::json` with `value()` defaults, but the schema version check may
reject them depending on consumer policy.
