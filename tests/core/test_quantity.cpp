#include <homestead/core/quantity.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace homestead;

TEST_CASE("VariableQuantity fixed constructor", "[quantity]") {
    VariableQuantity q{5.0};
    REQUIRE(q.min == 5.0);
    REQUIRE(q.expected == 5.0);
    REQUIRE(q.max == 5.0);
    REQUIRE(q.is_fixed());
}

TEST_CASE("VariableQuantity range constructor", "[quantity]") {
    VariableQuantity q{3.0, 5.0, 7.0};
    REQUIRE(q.min == 3.0);
    REQUIRE(q.expected == 5.0);
    REQUIRE(q.max == 7.0);
    REQUIRE_FALSE(q.is_fixed());
}

TEST_CASE("VariableQuantity default constructor gives zeros", "[quantity]") {
    VariableQuantity q;
    REQUIRE(q.min == 0.0);
    REQUIRE(q.expected == 0.0);
    REQUIRE(q.max == 0.0);
    REQUIRE(q.is_fixed());
}

TEST_CASE("VariableQuantity::make validates invariant", "[quantity]") {
    SECTION("valid range succeeds") {
        auto result = VariableQuantity::make(1.0, 3.0, 5.0);
        REQUIRE(result.has_value());
        REQUIRE(result->min == 1.0);
        REQUIRE(result->expected == 3.0);
        REQUIRE(result->max == 5.0);
    }

    SECTION("min > expected returns error") {
        auto result = VariableQuantity::make(5.0, 3.0, 7.0);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(!result.error().empty());
    }

    SECTION("expected > max returns error") {
        auto result = VariableQuantity::make(1.0, 8.0, 5.0);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("negative min returns error") {
        auto result = VariableQuantity::make(-1.0, 1.0, 2.0);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("all equal is valid fixed quantity") {
        auto result = VariableQuantity::make(4.0, 4.0, 4.0);
        REQUIRE(result.has_value());
        REQUIRE(result->is_fixed());
    }
}

TEST_CASE("MonthMask ALL_MONTHS has all 12 bits set", "[quantity][month]") {
    for (int m = 0; m < 12; ++m) {
        REQUIRE(is_active(ALL_MONTHS, m));
    }
}

TEST_CASE("MonthMask NO_MONTHS has no bits set", "[quantity][month]") {
    for (int m = 0; m < 12; ++m) {
        REQUIRE_FALSE(is_active(NO_MONTHS, m));
    }
}

TEST_CASE("MonthMask selective month activation", "[quantity][month]") {
    MonthMask jan_only = 0x0001u;  // bit 0 = January
    REQUIRE(is_active(jan_only, 0));
    for (int m = 1; m < 12; ++m) {
        REQUIRE_FALSE(is_active(jan_only, m));
    }

    MonthMask dec_only = 0x0800u;  // bit 11 = December
    REQUIRE(is_active(dec_only, 11));
    for (int m = 0; m < 11; ++m) {
        REQUIRE_FALSE(is_active(dec_only, m));
    }
}
