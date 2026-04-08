#pragma once

#include <homestead/core/quantity.hpp>

#include <optional>
#include <string>

namespace homestead {

/// Selects which point of the VariableQuantity triple the solver uses.
enum class Scenario {
    optimistic,  ///< Use the `min` value of each VariableQuantity
    expected,    ///< Use the `expected` value (default)
    pessimistic  ///< Use the `max` value
};

/// Tuning knobs for a single solver run.
struct SolverConfig {
    /// Which quantity triple point to use during calculations.
    Scenario scenario{Scenario::expected};
    /// If set, solver will not exceed this area (m²) for entity instances.
    std::optional<double> max_area_m2;
    /// If set, total infrastructure initial cost must not exceed this.
    std::optional<double> max_budget;
    /// If set, labor per month must not exceed this.
    std::optional<double> max_labor_hours_per_month;
    /// Maximum Gauss-Seidel iterations before declaring non-convergence.
    int max_convergence_iterations{100};
};

/// A single desired output from the homestead plan.
struct ProductionGoal {
    /// Resource to produce.
    std::string resource_slug;
    /// Required quantity per month. min ≥ 0.
    VariableQuantity quantity_per_month;
};

}  // namespace homestead
