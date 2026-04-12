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
