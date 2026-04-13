# Implementation Plan: Nutrient and Soil Balance Tracking (N-P-K)

**Branch**: `003-npk-soil-balance` | **Date**: 2026-04-09 | **Spec**: [spec.md](spec.md)

## Summary

Add a post-solve nutrient balance analytics layer that computes, per month, how
many grams of N, P, and K are supplied by fertilizer-producing entity instances
(net of intermediate processing) versus demanded by area-based crop entities.
Deficits trigger `DiagnosticKind::nutrient_deficit` entries in `PlanResult`.
No new library targets; no new dependencies. All changes are additive to existing
types in `homestead::core` and `homestead::solver`; the serialization module
handles the new fields with a schema bump to 1.1.0.

## Technical Context

**Language/Version**: C++23 (GCC 13+, Clang 17+, MSVC latest)  
**Primary Dependencies**: nlohmann/json 3.11+ (serialization), Catch2 v3.6+ (testing)  
**Storage**: JSON files for Registry and PlanResult persistence  
**Testing**: Catch2 v3 via `ctest --preset debug` / `--preset sanitize`  
**Target Platform**: Linux, macOS, Windows (CI matrix)  
**Project Type**: Library (`homestead::core`, `homestead::solver`, `homestead::serialization`)  
**Performance Goals**: Nutrient pass completes in < 1 ms per plan (post-solve, single-threaded scan)  
**Constraints**: No I/O in domain types; `homestead::core` must remain stdlib-only; no new external deps  
**Scale/Scope**: Same as solver — up to 10,000 graph nodes

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Modern C++23 Idioms | ✅ PASS | `std::optional`, `std::expected` used; no raw new/delete; ranges in composition iteration |
| II. Separation of Concerns | ✅ PASS | `NutrientDemand` in `core` (no I/O); analytics in `solver`; serialization in `serialization` |
| III. Testing (NON-NEGOTIABLE) | ✅ PASS | New test file + roundtrip assertions; Sisteminha integration test with fixed expected values |
| IV. Documentation | ✅ PASS | New public header `nutrient.hpp` requires Doxygen `///` comments on all exported types |
| V. Code Style | ✅ PASS | snake_case fields, PascalCase types; clang-format and clang-tidy must pass |
| VI. Build System | ✅ PASS | No new targets; existing `homestead::core`, `homestead::solver`, `homestead::serialization` targets |
| VII. CI | ✅ PASS | No change to CI matrix; new tests run in existing jobs including `sanitize` preset |
| VIII. Performance | ✅ PASS | O(E·M·R) scan (entities × months × resources); negligible vs solver fixed-point iteration |

No violations. Complexity Tracking table not required.

## Project Structure

### Documentation (this feature)

```text
specs/003-npk-soil-balance/
├── plan.md              ← this file
├── research.md          ← Phase 0 output
├── data-model.md        ← Phase 1 output
├── quickstart.md        ← Phase 1 output
├── contracts/
│   └── nutrient_api.md  ← Phase 1 output
└── tasks.md             ← Phase 2 output (/speckit.tasks — NOT created here)
```

### Source Code

```text
include/homestead/
├── core/
│   ├── entity.hpp           ← MODIFIED: add optional<NutrientDemand>
│   └── nutrient.hpp         ← NEW: NutrientDemand struct
└── solver/
    └── result.hpp           ← MODIFIED: NutrientBalance, nutrient_deficit, PlanResult field

src/
├── solver/
│   ├── analytics.cpp        ← MODIFIED: compute_nutrient_balance()
│   └── convergence.cpp      ← MODIFIED: check_nutrient_deficits()
└── serialization/
    └── serialization.cpp    ← MODIFIED: NutrientDemand, NutrientBalance, schema 1.1.0

data/
└── default_registry.json    ← MODIFIED: nutrient_demand on 6 area-based crop entities

tests/
├── solver/
│   └── test_nutrient_balance.cpp  ← NEW: 5 test cases
└── serialization/
    └── test_roundtrip.cpp         ← MODIFIED: nutrient_demand + nutrient_balance assertions
```

**Structure Decision**: Single-project, existing layout. All new code fits into existing
target boundaries. No new CMakeLists.txt changes needed beyond adding the new test file
to the solver test target.

## Detailed Implementation

### Step 1 — New header: `include/homestead/core/nutrient.hpp`

Create a stdlib-only header (no external deps, no I/O):

```cpp
#pragma once

namespace homestead {

/// Per-area, per-cycle nutrient demand for a crop entity.
/// All values in grams of element per m² per growing cycle.
/// Absent (std::nullopt on Entity) means the entity is excluded from
/// nutrient balance calculations.
struct NutrientDemand {
    double n_g_per_m2_per_cycle{};  ///< Nitrogen demand (g N / m² / cycle)
    double p_g_per_m2_per_cycle{};  ///< Phosphorus demand (g P / m² / cycle)
    double k_g_per_m2_per_cycle{};  ///< Potassium demand (g K / m² / cycle)
};

}  // namespace homestead
```

### Step 2 — Modify `include/homestead/core/entity.hpp`

Add one include and one field to `Entity`:

```cpp
#include <homestead/core/nutrient.hpp>
#include <optional>
// ...
struct Entity {
    // ... existing fields unchanged ...
    /// Nutrient demand per m² per cycle. Present only for area-based crop entities.
    /// Non-crop entities (animals, composters, fish tanks) leave this as nullopt.
    std::optional<NutrientDemand> nutrient_demand;
};
```

### Step 3 — Modify `include/homestead/solver/result.hpp`

#### 3a — Add `NutrientBalance` struct

```cpp
/// Monthly N-P-K supply and demand for a solved plan.
/// All values in grams of element per month.
struct NutrientBalance {
    MonthlyValues available_n{};  ///< Grams N supplied (net fertilizer output)
    MonthlyValues available_p{};  ///< Grams P supplied
    MonthlyValues available_k{};  ///< Grams K supplied
    MonthlyValues demanded_n{};   ///< Grams N demanded by crop entities
    MonthlyValues demanded_p{};   ///< Grams P demanded
    MonthlyValues demanded_k{};   ///< Grams K demanded
};
```

#### 3b — Add `nutrient_deficit` to `DiagnosticKind`

```cpp
enum class DiagnosticKind {
    unsatisfied_goal,
    area_exceeded,
    non_convergent_cycle,
    missing_producer,
    seasonality_gap,
    nutrient_deficit   ///< N, P, or K demand exceeds available supply in a month
};
```

#### 3c — Add `nutrient_balance` to `PlanResult`

```cpp
struct PlanResult {
    // ... existing fields ...
    /// Nutrient supply/demand balance. nullopt when no crop entity instances
    /// are present in the solved graph (nothing to balance).
    std::optional<NutrientBalance> nutrient_balance;
};
```

### Step 4 — Add `compute_nutrient_balance()` to `src/solver/analytics.cpp`

Add before `populate_plan_result()`. Uses both the graph (for entity instance
quantities and crop inputs) and the balance_sheet (for gross fertilizer production).

#### 4a — Composition extraction helper (internal linkage)

```cpp
// Returns grams of N, P, K from a resource's composition map and a net quantity.
// unit_is_kg: true for solid resources (N_percent keys), false for liquids (N_ppm keys).
struct NpkGrams { double n{}, p{}, k{}; };

NpkGrams extract_npk(const std::unordered_map<std::string, double>& composition,
                     double net_quantity, bool unit_is_kg) noexcept {
    NpkGrams g;
    if (unit_is_kg) {
        // N_percent = % of dry weight → grams = kg × percent/100 × 1000
        if (auto it = composition.find("N_percent"); it != composition.end())
            g.n = net_quantity * it->second / 100.0 * 1000.0;
        if (auto it = composition.find("P_percent"); it != composition.end())
            g.p = net_quantity * it->second / 100.0 * 1000.0;
        if (auto it = composition.find("K_percent"); it != composition.end())
            g.k = net_quantity * it->second / 100.0 * 1000.0;
    } else {
        // N_ppm = mg/L → grams = liters × ppm / 1000
        if (auto it = composition.find("N_ppm"); it != composition.end())
            g.n = net_quantity * it->second / 1000.0;
        if (auto it = composition.find("P_ppm"); it != composition.end())
            g.p = net_quantity * it->second / 1000.0;
        // K_ppm absent (e.g., biofertilizer_l) → g.k remains 0.0 (Q3 default)
    }
    return g;
}
```

Unit detection: a resource uses ppm keys if its `composition` map contains any
key ending in `_ppm`; otherwise assume `_percent` / kg-based.

#### 4b — Net supply computation

```cpp
std::optional<NutrientBalance> compute_nutrient_balance(
    const Graph& graph,
    const Registry& registry,
    const std::vector<ResourceBalance>& balance_sheet,
    Scenario scenario) {

    // ── Supply side ────────────────────────────────────────────────────────
    // Net output of a fertilizer resource = gross internal production
    // minus the quantity consumed by intermediate (non-crop) entity instances.
    // This prevents double-counting nutrients that flow through transformation
    // steps (e.g., compost → earthworm bin → vermicompost).

    // Build a map: resource_slug → gross production per month (from balance_sheet)
    std::unordered_map<std::string, MonthlyValues> gross_prod;
    for (const auto& bal : balance_sheet) {
        gross_prod[bal.resource_slug] = bal.internal_production;
    }

    // Build: resource_slug → grams consumed by non-crop entities per month
    std::unordered_map<std::string, MonthlyValues> feedstock_consumed;
    graph.for_each_node([&](NodeId, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv)) return;
        const auto& inst = std::get<EntityInstanceNode>(nv);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt || entity_opt->nutrient_demand.has_value()) return; // skip crops
        double qty = detail::pick(inst.quantity, scenario);
        for (std::size_t m = 0; m < 12; ++m) {
            double cpm = cycles_in_month(entity_opt->lifecycle, static_cast<int>(m));
            for (const auto& inp : entity_opt->inputs) {
                feedstock_consumed[inp.resource_slug][m] +=
                    detail::pick(inp.quantity_per_cycle, scenario) * cpm * qty;
            }
        }
    });

    NutrientBalance balance;
    bool has_fertilizer = false;

    for (const auto& [slug, prod_monthly] : gross_prod) {
        auto res_opt = registry.find_resource(slug);
        if (!res_opt || res_opt->composition.empty()) continue;
        // Check if this resource has any N/P/K composition keys
        const auto& comp = res_opt->composition;
        bool has_npk = comp.contains("N_percent") || comp.contains("N_ppm") ||
                       comp.contains("P_percent") || comp.contains("P_ppm") ||
                       comp.contains("K_percent");
        if (!has_npk) continue;

        bool is_liquid = comp.contains("N_ppm") || comp.contains("P_ppm");
        const auto& feedstock = feedstock_consumed[slug]; // default-inits to zeros

        for (std::size_t m = 0; m < 12; ++m) {
            double net = std::max(0.0, prod_monthly[m] - feedstock[m]);
            if (net <= 0.0) continue;
            has_fertilizer = true;
            auto npk = extract_npk(comp, net, !is_liquid);
            balance.available_n[m] += npk.n;
            balance.available_p[m] += npk.p;
            balance.available_k[m] += npk.k;
        }
    }

    // ── Demand side ────────────────────────────────────────────────────────
    bool has_demand = false;
    graph.for_each_node([&](NodeId, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv)) return;
        const auto& inst = std::get<EntityInstanceNode>(nv);
        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt || !entity_opt->nutrient_demand) return;

        const auto& nd = *entity_opt->nutrient_demand;
        double qty    = detail::pick(inst.quantity, scenario);
        double area   = detail::pick(entity_opt->infrastructure.area_m2, scenario);

        for (std::size_t m = 0; m < 12; ++m) {
            double frac = cycles_in_month(entity_opt->lifecycle, static_cast<int>(m));
            // frac = cycles_per_year / active_month_count — proportional distribution
            // across active months satisfies the prorated Q1 requirement.
            double scale = area * qty * frac;
            balance.demanded_n[m] += nd.n_g_per_m2_per_cycle * scale;
            balance.demanded_p[m] += nd.p_g_per_m2_per_cycle * scale;
            balance.demanded_k[m] += nd.k_g_per_m2_per_cycle * scale;
            if (scale > 0.0) has_demand = true;
        }
    });

    if (!has_demand && !has_fertilizer) return std::nullopt;
    return balance;
}
```

#### 4c — Call from `populate_plan_result()`

Add after `check_labor_constraint`:

```cpp
result.nutrient_balance = compute_nutrient_balance(
    result.graph, registry, result.balance_sheet, config.scenario);
```

### Step 5 — Add `check_nutrient_deficits()` to `src/solver/convergence.cpp`

```cpp
void check_nutrient_deficits(const std::optional<NutrientBalance>& balance,
                             std::vector<Diagnostic>& diagnostics) {
    if (!balance) return;
    static constexpr std::array<const char*, 12> month_names = {
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

    struct ElementView {
        const char* name;
        const MonthlyValues& available;
        const MonthlyValues& demanded;
    };
    std::array views{
        ElementView{"Nitrogen",   balance->available_n, balance->demanded_n},
        ElementView{"Phosphorus", balance->available_p, balance->demanded_p},
        ElementView{"Potassium",  balance->available_k, balance->demanded_k},
    };

    for (const auto& [elem, avail, demand] : views) {
        for (int m = 0; m < 12; ++m) {
            if (demand[m] <= avail[m]) continue;
            double shortfall = demand[m] - avail[m];
            diagnostics.push_back(Diagnostic{
                DiagnosticKind::nutrient_deficit,
                std::format("{} deficit in {}: {:.1f} g available vs {:.1f} g demanded "
                            "(shortfall: {:.1f} g)",
                            elem, month_names[m], avail[m], demand[m], shortfall),
                std::nullopt, std::nullopt});
        }
    }
}
```

Call from `populate_plan_result()` after `compute_nutrient_balance`:

```cpp
check_nutrient_deficits(result.nutrient_balance, result.diagnostics);
```

### Step 6 — Serialization (`src/serialization/serialization.cpp`)

#### 6a — Bump schema version constant

In `include/homestead/serialization/schema_version.hpp` (or wherever
`PLAN_SCHEMA_VERSION` is defined):

```cpp
inline constexpr SchemaVersion PLAN_SCHEMA_VERSION{1, 1, 0};
```

#### 6b — Serialize `NutrientDemand` in `entity_to_json()` / `entity_from_json()`

```cpp
// In entity_to_json():
if (e.nutrient_demand) {
    const auto& nd = *e.nutrient_demand;
    j["nutrient_demand"]["n_g_per_m2_per_cycle"] = nd.n_g_per_m2_per_cycle;
    j["nutrient_demand"]["p_g_per_m2_per_cycle"] = nd.p_g_per_m2_per_cycle;
    j["nutrient_demand"]["k_g_per_m2_per_cycle"] = nd.k_g_per_m2_per_cycle;
}

// In entity_from_json():
if (j.contains("nutrient_demand") && j["nutrient_demand"].is_object()) {
    NutrientDemand nd;
    const auto& ndj = j["nutrient_demand"];
    nd.n_g_per_m2_per_cycle = ndj.value("n_g_per_m2_per_cycle", 0.0);
    nd.p_g_per_m2_per_cycle = ndj.value("p_g_per_m2_per_cycle", 0.0);
    nd.k_g_per_m2_per_cycle = ndj.value("k_g_per_m2_per_cycle", 0.0);
    e.nutrient_demand = nd;
}
```

#### 6c — Serialize `NutrientBalance` in plan result serializer

```cpp
// Serialize:
if (result.nutrient_balance) {
    const auto& nb = *result.nutrient_balance;
    j["nutrient_balance"]["available_n"] = nb.available_n;
    j["nutrient_balance"]["available_p"] = nb.available_p;
    j["nutrient_balance"]["available_k"] = nb.available_k;
    j["nutrient_balance"]["demanded_n"]  = nb.demanded_n;
    j["nutrient_balance"]["demanded_p"]  = nb.demanded_p;
    j["nutrient_balance"]["demanded_k"]  = nb.demanded_k;
}
// If nullopt, field is simply omitted from JSON.

// Deserialize — Q2 rule:
// Schema < 1.1.0 OR field absent → zero-initialized NutrientBalance{} (NOT nullopt).
// nullopt only when field is explicitly set to null in JSON (future: no-crop plan marker).
if (j.contains("nutrient_balance") && j["nutrient_balance"].is_object()) {
    NutrientBalance nb;
    const auto& nbj = j["nutrient_balance"];
    nb.available_n = nbj.value("available_n", MonthlyValues{});
    nb.available_p = nbj.value("available_p", MonthlyValues{});
    nb.available_k = nbj.value("available_k", MonthlyValues{});
    nb.demanded_n  = nbj.value("demanded_n",  MonthlyValues{});
    nb.demanded_p  = nbj.value("demanded_p",  MonthlyValues{});
    nb.demanded_k  = nbj.value("demanded_k",  MonthlyValues{});
    result.nutrient_balance = nb;
} else if (parsed_version < SchemaVersion{1, 1, 0}) {
    // Old plan file: zero-initialize (Q2 clarification — NOT nullopt)
    result.nutrient_balance = NutrientBalance{};
}
// If field absent in a 1.1.0+ file: remains nullopt (no-crop plan)
```

#### 6d — Add `nutrient_deficit` to `DiagnosticKind` serializer switch

```cpp
case DiagnosticKind::nutrient_deficit: return "nutrient_deficit";
// and the reverse lookup in from_string / deserialization.
```

### Step 7 — `data/default_registry.json` updates

Add `nutrient_demand` to the six area-based crop entities. Values in
g/m²/cycle from Embrapa technical bulletins (Circular Técnica series).
Add a `"_nutrient_source"` metadata key as documentation:

```json
"nutrient_demand": {
    "n_g_per_m2_per_cycle": 10.0,
    "p_g_per_m2_per_cycle": 4.0,
    "k_g_per_m2_per_cycle": 8.0,
    "_source": "Embrapa Hortaliças, Circular Técnica 40"
}
```

| Entity slug | N (g/m²/cycle) | P (g/m²/cycle) | K (g/m²/cycle) | Source note |
|---|---|---|---|---|
| `lettuce_bed_1m2` | 10.0 | 4.0 | 8.0 | Embrapa Hortaliças CT-40 |
| `tomato_bed_1m2` | 15.0 | 6.0 | 12.0 | Embrapa Hortaliças CT-57 |
| `pepper_bed_1m2` | 12.0 | 5.0 | 10.0 | Embrapa Hortaliças CT-27 |
| `corn_plot_1m2` | 8.0 | 3.0 | 6.0 | Embrapa Milho CT-78 |
| `bean_plot_1m2` | 4.0 | 3.0 | 5.0 | Embrapa Arroz e Feijão CT-19 (N lower: BNF) |
| `cassava_plot_1m2` | 5.0 | 2.0 | 8.0 | Embrapa Mandioca CT-12 (K-hungry) |

Do NOT add `nutrient_demand` to `banana_plant`, `papaya_plant`, or
`acerola_plant` — these are per-plant entities, not area-based. Marked
as future work in a TODO comment in the registry file.

### Step 8 — New test file: `tests/solver/test_nutrient_balance.cpp`

Five test cases:

**Test 1 — Classic Sisteminha supply is nonzero**  
Plan: chicken coop + compost bin + tilapia tank + lettuce bed + corn plot.  
Assert: `nutrient_balance.has_value()`, `available_n[*] > 0` for months
when compost is produced, `demanded_n[*] > 0` for crop-active months.

**Test 2 — Nitrogen deficit diagnostic**  
Plan: large corn plot (10 m²) + minimal chicken coop (1 bird).  
Hand-calculated expected:
- Monthly compost output ≈ small → available_N per month  
- Corn demand: 8.0 g/m²/cycle × 10 m² × 1 instance × cycles_in_month  
Assert: at least one `DiagnosticKind::nutrient_deficit` emitted for
nitrogen; diagnostic message contains "Nitrogen" and "shortfall".

**Test 3 — No crop entities → `nutrient_balance` is `nullopt`**  
Plan: broiler chickens + tilapia only (no crop entities).  
Assert: `!result.nutrient_balance.has_value()`.

**Test 4 — Unit conversion correctness**  
Manually construct:
- Solid resource with `N_percent = 1.5`, produce 100 kg net
  → expected available_N = 100 × 1.5/100 × 1000 = 1500 g
- Liquid resource with `N_ppm = 30.0`, produce 1000 L net
  → expected available_N = 1000 × 30.0/1000 = 30 g
Assert within 1e-9 tolerance.

**Test 5 — JSON round-trip**  
Produce a `PlanResult` with non-trivial `nutrient_balance`, serialize
to JSON string, deserialize, compare all six `MonthlyValues` arrays
element-by-element within `1e-9` tolerance.

Also add: **Test 6 — Schema 1.0.x backward compat**  
Load a hard-coded JSON string with `"version": "1.0.0"` and no
`nutrient_balance` field. Deserialize. Assert:
`result.nutrient_balance.has_value()` is **true** and all arrays are
zero (Q2 clarification — zero-init, not nullopt).

### Step 9 — Modify `tests/serialization/test_roundtrip.cpp`

Add roundtrip assertions for:
1. `Entity` with `nutrient_demand` set → JSON → `Entity` with identical `nutrient_demand`.
2. `Entity` with `nutrient_demand = nullopt` → JSON → no `nutrient_demand` key in JSON.
3. `PlanResult` with `nutrient_balance` (non-trivial values) round-trips losslessly.

## Complexity Tracking

No violations found. No entries needed.
