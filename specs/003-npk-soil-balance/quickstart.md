# Quickstart: Nutrient Balance (003-npk-soil-balance)

## Reading the nutrient balance after a solve

```cpp
#include <homestead/solver/solver.hpp>
#include <homestead/solver/result.hpp>

// ... set up registry, goals, config ...
auto result = solver.solve(goals, config);

if (result.nutrient_balance) {
    const auto& nb = *result.nutrient_balance;
    // Check January nitrogen balance (index 0)
    double jan_deficit_n = nb.demanded_n[0] - nb.available_n[0];
    if (jan_deficit_n > 0.0) {
        // Nitrogen shortfall in January
    }
}

// Or just check diagnostics directly:
for (const auto& d : result.diagnostics) {
    if (d.kind == DiagnosticKind::nutrient_deficit) {
        std::println("{}", d.message);
        // "Nitrogen deficit in Jan: 12.0 g available vs 26.7 g demanded (shortfall: 14.7 g)"
    }
}
```

## Adding nutrient demand to a crop entity (registry JSON)

```json
{
    "slug": "lettuce_bed_1m2",
    "name": "Lettuce Bed (1 m²)",
    "nutrient_demand": {
        "n_g_per_m2_per_cycle": 10.0,
        "p_g_per_m2_per_cycle": 4.0,
        "k_g_per_m2_per_cycle": 8.0,
        "_source": "Embrapa Hortaliças, Circular Técnica 40"
    }
}
```

## Adding N-P-K composition to a fertilizer resource (registry JSON)

Solid resource (% dry weight):
```json
{
    "slug": "my_compost_kg",
    "composition": {
        "N_percent": 1.5,
        "P_percent": 0.8,
        "K_percent": 1.2
    }
}
```

Liquid resource (mg/L):
```json
{
    "slug": "my_liquid_fertilizer_l",
    "composition": {
        "N_ppm": 500.0,
        "P_ppm": 150.0
    }
}
```

Missing elements (e.g., no `K_ppm`) default to 0 — no error.

## Build and test

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug -R test_nutrient_balance   # new test file only
ctest --preset debug                             # full suite
ctest --preset sanitize                          # ASan + UBSan
```
