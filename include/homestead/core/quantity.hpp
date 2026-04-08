#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace homestead {

/// Three-point estimate for any measurable quantity (min, expected, max).
/// All components must satisfy: 0 ≤ min ≤ expected ≤ max.
struct VariableQuantity {
    /// Lower bound — worst-case or minimum realistic value.
    double min{};
    /// Central estimate — the value used for planning calculations.
    double expected{};
    /// Upper bound — best-case or maximum realistic value.
    double max{};

    /// Constructs a fixed quantity where min = expected = max = value.
    explicit VariableQuantity(double fixed) noexcept : min{fixed}, expected{fixed}, max{fixed} {}

    /// Constructs a range quantity. Use make() for validated construction.
    VariableQuantity(double min_val, double expected_val, double max_val) noexcept
        : min{min_val}, expected{expected_val}, max{max_val} {}

    /// Default construct (all zeros).
    VariableQuantity() noexcept = default;

    /// Validated factory. Returns error string if 0 ≤ min ≤ expected ≤ max is violated.
    [[nodiscard]] static std::expected<VariableQuantity, std::string> make(double min_val,
                                                                           double expected_val,
                                                                           double max_val) noexcept;

    /// Returns true when min == expected == max.
    [[nodiscard]] bool is_fixed() const noexcept { return min == expected && expected == max; }
};

// ── MonthMask ──────────────────────────────────────────────────────────────────

/// A bitmask representing a set of calendar months.
/// Bit 0 = January, bit 11 = December.
using MonthMask = std::uint16_t;

/// All twelve months active.
inline constexpr MonthMask ALL_MONTHS = 0x0FFFU;
/// No months active.
inline constexpr MonthMask NO_MONTHS = 0x0000U;

/// Returns true if the given month (0 = Jan, 11 = Dec) is set in mask.
[[nodiscard]] inline constexpr bool is_active(MonthMask mask, int month) noexcept {
    return (mask & (static_cast<MonthMask>(1U) << month)) != 0;
}

}  // namespace homestead
