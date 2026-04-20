#pragma once

#include <homestead/core/nutrient.hpp>
#include <homestead/core/quantity.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace homestead {

/// Temporal lifecycle parameters for an entity type.
struct Lifecycle {
    /// Days required to set up before first cycle begins. ≥ 0.
    int setup_days{};
    /// Duration of one production cycle in days. > 0.
    int cycle_days{1};
    /// Expected number of complete cycles per year. > 0.
    double cycles_per_year{1.0};
    /// Months in which this entity is operationally active (bitmask).
    MonthMask active_months{ALL_MONTHS};
};

/// Specifies a single resource input or output for one cycle of an entity.
struct ResourceFlowSpec {
    /// Resource being consumed (input) or produced (output).
    std::string resource_slug;
    /// Quantity per cycle: all components > 0 for outputs; ≥ 0 for inputs.
    VariableQuantity quantity_per_cycle;
    /// When within the cycle the flow occurs. 0.0 = start, 1.0 = end.
    double timing_within_cycle{0.0};
};

/// Infrastructure requirements for setting up and running an entity.
struct InfrastructureSpec {
    /// Floor area required (m²). min ≥ 0.
    VariableQuantity area_m2;
    /// Construction material flows (consumed once at setup).
    std::vector<ResourceFlowSpec> construction_materials;
    /// Labor hours required for initial setup. min ≥ 0.
    VariableQuantity initial_labor_hours;
    /// Monetary cost of initial setup. min ≥ 0.
    VariableQuantity initial_cost;
};

/// An entity template in the Registry.
/// Entities are the production units of a homestead plan.
struct Entity {
    /// Unique identifier: lowercase letters, digits, underscores only.
    std::string slug;
    /// Human-readable display name.
    std::string name;
    /// Optional longer description.
    std::string description;
    /// Resources consumed each cycle. All resource_slugs must exist in the Registry.
    std::vector<ResourceFlowSpec> inputs;
    /// Resources produced each cycle. Must have ≥ 1 entry. All slugs must exist.
    std::vector<ResourceFlowSpec> outputs;
    /// Lifecycle parameters.
    Lifecycle lifecycle;
    /// Labor required to operate this entity each cycle. min ≥ 0.
    VariableQuantity operating_labor_per_cycle;
    /// Stocking density (units per m²). min ≥ 0.
    VariableQuantity stocking_density;
    /// Infrastructure requirements.
    InfrastructureSpec infrastructure;
    /// Nutrient demand per m² per cycle. Present only for area-based crop entities.
    /// Non-crop entities (animals, composters, fish tanks) leave this as nullopt.
    std::optional<NutrientDemand> nutrient_demand;

    /// Nutritional or chemical amounts this entity needs per cycle, without naming a
    /// specific resource slug. Keys use canonical gram-per-unit names: "protein_g",
    /// "energy_kcal", "N_g", "P_g", "K_g". The solver's composition-matching pass
    /// routes available resources to satisfy these needs (FR-001, FR-003).
    /// Empty map = no composition-based feed requirements (slug-only entity).
    /// Invariant: all values > 0.0.
    std::unordered_map<std::string, double> composition_requirements;

    /// Fertilization need per square metre of planted area per cycle.
    /// Keys: "N_g", "P_g", "K_g" (grams per m² per cycle).
    /// Scaled at solve time by infrastructure.area_m2 × instance_count × cpm (FR-002).
    /// Empty map = no composition-based fertilization requirements.
    /// Invariant: all values > 0.0.
    std::unordered_map<std::string, double> fertilization_per_m2;
};

}  // namespace homestead
