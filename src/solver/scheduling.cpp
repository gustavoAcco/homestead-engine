// Monthly scheduling helpers.
// Distributes entity-instance production across calendar months based on
// Lifecycle::active_months and cycles_per_year (research.md §5).

#include <homestead/solver/solver.hpp>

#include <cmath>

namespace homestead {

/// Compute how many cycles an entity runs in a specific month.
double cycles_in_month(const Lifecycle& lc, int month) noexcept {
    if (!is_active(lc.active_months, month)) {
        return 0.0;
    }
    int active_count = 0;
    for (int m = 0; m < 12; ++m) {
        if (is_active(lc.active_months, m)) {
            ++active_count;
        }
    }
    if (active_count == 0) {
        return 0.0;
    }
    return lc.cycles_per_year / static_cast<double>(active_count);
}

/// Build a 12-element labor schedule (hours/month) for a single entity instance.
MonthlyValues labor_for_instance(const Entity& entity, double instance_quantity,
                                 Scenario scenario) noexcept {
    MonthlyValues schedule{};
    double labor_per_cycle = [&]() {
        switch (scenario) {
            case Scenario::optimistic:
                return entity.operating_labor_per_cycle.min;
            case Scenario::pessimistic:
                return entity.operating_labor_per_cycle.max;
            default:
                return entity.operating_labor_per_cycle.expected;
        }
    }();

    for (std::size_t m = 0; m < 12; ++m) {
        double cycles = cycles_in_month(entity.lifecycle, static_cast<int>(m));
        schedule[m] = labor_per_cycle * cycles * instance_quantity;
    }
    return schedule;
}

}  // namespace homestead
