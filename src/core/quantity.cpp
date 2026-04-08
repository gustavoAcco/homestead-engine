#include <homestead/core/quantity.hpp>

#include <format>

namespace homestead {

std::expected<VariableQuantity, std::string> VariableQuantity::make(double min_val,
                                                                    double expected_val,
                                                                    double max_val) noexcept {
    if (min_val < 0.0) {
        return std::unexpected(std::format("VariableQuantity: min ({}) must be >= 0", min_val));
    }
    if (min_val > expected_val) {
        return std::unexpected(
            std::format("VariableQuantity: min ({}) > expected ({})", min_val, expected_val));
    }
    if (expected_val > max_val) {
        return std::unexpected(
            std::format("VariableQuantity: expected ({}) > max ({})", expected_val, max_val));
    }
    return VariableQuantity{min_val, expected_val, max_val};
}

}  // namespace homestead
