// Internal helper — not part of the public homestead API.
// Extracts the scenario-selecting logic shared by backpropagation.cpp
// and analytics.cpp so it can be changed in one place.
#pragma once

#include <homestead/core/quantity.hpp>
#include <homestead/solver/config.hpp>

namespace homestead::detail {

/// Returns the scenario-appropriate point from a VariableQuantity triple.
[[nodiscard]] inline double pick(const VariableQuantity& q, Scenario s) noexcept {
    switch (s) {
        case Scenario::optimistic:
            return q.min;
        case Scenario::expected:
            return q.expected;
        case Scenario::pessimistic:
            return q.max;
    }
    return q.expected;
}

}  // namespace homestead::detail
