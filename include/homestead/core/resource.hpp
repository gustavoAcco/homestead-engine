#pragma once

#include <homestead/core/quantity.hpp>

#include <optional>
#include <string>
#include <unordered_map>

namespace homestead {

/// Classification of what kind of thing a Resource represents.
enum class ResourceCategory {
    raw_material,       ///< Unprocessed natural inputs (wood, stone, soil)
    feed,               ///< Animal feed
    fertilizer,         ///< Soil amendment
    food_product,       ///< Human-consumable food
    building_material,  ///< Construction inputs
    fuel,               ///< Energy carriers (biogas, firewood)
    water,              ///< Water in any form
    labor_hours,        ///< Human labor time
    money,              ///< Currency / economic value
    waste,              ///< Waste streams (manure, compost feedstock)
    other               ///< Uncategorized
};

/// Chemical composition as element/compound slug → quantity.
/// Units are context-dependent (% dry weight, mg/kg, etc.).
using ChemicalComposition = std::unordered_map<std::string, double>;

/// Nutritional profile — present only for food_product and feed resources.
struct NutritionalProfile {
    /// Energy content in kcal per kg.
    double calories_kcal_per_kg{};
    /// Protein in grams per kg.
    double protein_g_per_kg{};
    /// Fat in grams per kg.
    double fat_g_per_kg{};
    /// Carbohydrates in grams per kg.
    double carbs_g_per_kg{};
    /// Micronutrients by slug (mg/kg).
    std::unordered_map<std::string, double> micronutrients;
};

/// Physical characteristics of a resource.
struct PhysicalProperties {
    /// Weight in kg per base unit of the resource. Must be > 0.
    double weight_kg_per_unit{1.0};
    /// Volume in litres per base unit. 0 = not applicable.
    double volume_liters_per_unit{};
    /// Shelf life in days. -1 = indefinite (no expiry).
    int shelf_life_days{-1};
};

/// A resource template in the Registry.
/// Resources are identified by their slug and referenced by slug from Entity flows.
struct Resource {
    /// Unique identifier: lowercase letters, digits, underscores only.
    std::string slug;
    /// Human-readable display name.
    std::string name;
    /// Classification.
    ResourceCategory category{ResourceCategory::other};
    /// Optional chemical composition data.
    ChemicalComposition composition;
    /// Optional nutritional data (food_product / feed only).
    std::optional<NutritionalProfile> nutrition;
    /// Physical properties (required).
    PhysicalProperties physical;
};

/// Returns true if the slug consists only of [a-z0-9_] and is non-empty.
[[nodiscard]] bool is_valid_slug(std::string_view slug) noexcept;

}  // namespace homestead
