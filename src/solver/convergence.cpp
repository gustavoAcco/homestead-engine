// Fixed-point convergence helpers.
// The Gauss-Seidel loop itself lives in backpropagation.cpp; this file
// provides the seasonality and labour-constraint checks that are run
// after the graph is fully built.

#include <homestead/solver/solver.hpp>

#include <array>
#include <format>

namespace homestead {

/// Append DiagnosticKind::seasonality_gap entries for any goal whose required
/// resource has no active producer in one or more of the goal's months.
void check_seasonality(const std::vector<ProductionGoal>& goals, const Registry& registry,
                       const Graph& graph, std::vector<Diagnostic>& diagnostics) {
    static constexpr std::array<const char*, 12> month_names = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    for (const auto& goal : goals) {
        auto producers = registry.producers_of(goal.resource_slug);
        if (producers.empty()) {
            continue;
        }

        for (int m = 0; m < 12; ++m) {
            // Goals use ALL_MONTHS semantics; any non-zero quantity_per_month
            // applies to all months.
            if (goal.quantity_per_month.expected <= 0.0) {
                continue;
            }

            // Check whether at least one producer entity is active this month.
            bool covered = false;
            for (const auto& prod_slug : producers) {
                auto entity_opt = registry.find_entity(prod_slug);
                if (!entity_opt) {
                    continue;
                }
                if (is_active(entity_opt->lifecycle.active_months, m)) {
                    covered = true;
                    break;
                }
            }
            if (!covered) {
                diagnostics.push_back(Diagnostic{
                    DiagnosticKind::seasonality_gap,
                    std::format("Resource '{}' has no active producer in {}", goal.resource_slug,
                                month_names[static_cast<std::size_t>(m)]),
                    goal.resource_slug, std::nullopt});
                break;  // One diagnostic per resource is enough.
            }
        }
    }
}

/// Append DiagnosticKind::unsatisfied_goal entries for months where the
/// plan's total labour exceeds max_labor_hours_per_month.
void check_labor_constraint(const MonthlyValues& labor_schedule, double max_labor_per_month,
                            std::vector<Diagnostic>& diagnostics) {
    static constexpr std::array<const char*, 12> month_names = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    for (std::size_t m = 0; m < 12; ++m) {
        if (labor_schedule[m] > max_labor_per_month) {
            diagnostics.push_back(
                Diagnostic{DiagnosticKind::unsatisfied_goal,
                           std::format("Labor constraint exceeded in {}: {:.1f} h > {:.1f} h limit",
                                       month_names[m], labor_schedule[m], max_labor_per_month),
                           std::nullopt, std::nullopt});
        }
    }
}

}  // namespace homestead
