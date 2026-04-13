#pragma once

#include <homestead/graph/graph.hpp>

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace homestead {

/// Monthly quantity array — index 0 = January, index 11 = December.
using MonthlyValues = std::array<double, 12>;

/// Resource balance sheet for one resource across all 12 months.
struct ResourceBalance {
    /// Resource identifier.
    std::string resource_slug;
    /// Produced internally by plan entity instances (kg, L, etc. per month).
    MonthlyValues internal_production{};
    /// Consumed by plan entity instances and goals.
    MonthlyValues consumption{};
    /// Purchased from external sources.
    MonthlyValues external_purchase{};
    /// Annualised totals.
    double annual_internal_production{};
    double annual_consumption{};
    double annual_external_purchase{};
};

/// Classification of solver diagnostic messages.
enum class DiagnosticKind {
    unsatisfied_goal,      ///< Goal cannot be met (area/budget/no producer)
    area_exceeded,         ///< Plan exceeds max_area_m2 constraint
    non_convergent_cycle,  ///< Fixed-point did not converge within iteration limit
    missing_producer,      ///< No registry entity produces a required resource
    seasonality_gap,       ///< Goal resource unavailable in one or more goal months
    nutrient_deficit       ///< N, P, or K demand exceeds available supply in a month
};

/// Returns a human-readable label for a DiagnosticKind.
[[nodiscard]] std::string to_string(DiagnosticKind kind);

/// Monthly N-P-K supply and demand for a solved plan.
/// All values in grams of element per month.
/// Index 0 = January, index 11 = December.
struct NutrientBalance {
    MonthlyValues available_n{};  ///< Grams N supplied (net fertilizer output)
    MonthlyValues available_p{};  ///< Grams P supplied
    MonthlyValues available_k{};  ///< Grams K supplied
    MonthlyValues demanded_n{};   ///< Grams N demanded by crop entities
    MonthlyValues demanded_p{};   ///< Grams P demanded
    MonthlyValues demanded_k{};   ///< Grams K demanded
};

/// A single solver diagnostic entry.
struct Diagnostic {
    /// Classification.
    DiagnosticKind kind;
    /// Human-readable description.
    std::string message;
    /// Affected resource slug, if applicable.
    std::optional<std::string> resource_slug;
    /// Affected entity slug, if applicable.
    std::optional<std::string> entity_slug;
};

/// Bill of materials for all infrastructure in the plan.
struct InfrastructureBOM {
    /// Total floor area required across all entity instances.
    double total_area_m2{};
    /// Per-resource material quantities: (resource_slug, total_quantity).
    std::vector<std::pair<std::string, double>> materials;
    /// Estimated total initial monetary cost.
    double estimated_initial_cost{};
    /// Total setup labor hours across all instances.
    double initial_labor_hours{};
};

/// The complete output of a solver run.
struct PlanResult {
    /// Fully resolved resource-flow graph with all entity instances and sources.
    Graph graph;
    /// Per-resource monthly balance sheet.
    std::vector<ResourceBalance> balance_sheet;
    /// Subset of balance_sheet where annual_external_purchase > 0.
    std::vector<ResourceBalance> gap_report;
    /// Total labor-hours per month across all entity instances.
    MonthlyValues labor_schedule{};
    /// Infrastructure bill of materials.
    InfrastructureBOM bom;
    /// Fraction of resource demand satisfied internally. [0.0, 1.0].
    double loop_closure_score{};
    /// Solver messages. Empty implies all goals were fully satisfied.
    std::vector<Diagnostic> diagnostics;
    /// Nutrient supply/demand balance. nullopt when no crop entity instances
    /// are present in the solved graph (nothing to balance).
    std::optional<NutrientBalance> nutrient_balance;
};

}  // namespace homestead
