#include <homestead/core/resource.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace homestead;

TEST_CASE("is_valid_slug accepts valid slugs", "[resource][slug]") {
    REQUIRE(is_valid_slug("corn_grain_kg"));
    REQUIRE(is_valid_slug("broiler_chicken"));
    REQUIRE(is_valid_slug("co2"));
    REQUIRE(is_valid_slug("a"));
    REQUIRE(is_valid_slug("abc123"));
    REQUIRE(is_valid_slug("x_1_y"));
}

TEST_CASE("is_valid_slug rejects invalid slugs", "[resource][slug]") {
    REQUIRE_FALSE(is_valid_slug(""));
    REQUIRE_FALSE(is_valid_slug("Corn_Grain_Kg"));    // uppercase
    REQUIRE_FALSE(is_valid_slug("corn-grain"));       // hyphen
    REQUIRE_FALSE(is_valid_slug("corn grain"));       // space
    REQUIRE_FALSE(is_valid_slug("broiler.chicken"));  // dot
    REQUIRE_FALSE(is_valid_slug("123ABC"));           // uppercase
}

TEST_CASE("Resource can be constructed with minimal fields", "[resource]") {
    Resource r;
    r.slug = "test_resource";
    r.name = "Test Resource";
    r.category = ResourceCategory::other;
    r.physical = PhysicalProperties{1.0, 0.0, -1};

    REQUIRE(r.slug == "test_resource");
    REQUIRE(r.name == "Test Resource");
    REQUIRE(r.category == ResourceCategory::other);
    REQUIRE_FALSE(r.nutrition.has_value());
    REQUIRE(r.composition.empty());
}

TEST_CASE("Resource with NutritionalProfile", "[resource]") {
    Resource r;
    r.slug = "egg";
    r.name = "Egg";
    r.category = ResourceCategory::food_product;
    r.nutrition = NutritionalProfile{1550.0, 125.0, 110.0, 12.0, {}};
    r.physical = PhysicalProperties{0.06, 0.0, 21};

    REQUIRE(r.nutrition.has_value());
    REQUIRE(r.nutrition->protein_g_per_kg == 125.0);
    REQUIRE(r.nutrition->calories_kcal_per_kg == 1550.0);
}

TEST_CASE("ResourceCategory enum covers expected values", "[resource]") {
    REQUIRE(static_cast<int>(ResourceCategory::food_product) >= 0);
    REQUIRE(static_cast<int>(ResourceCategory::waste) >= 0);
    REQUIRE(static_cast<int>(ResourceCategory::water) >= 0);
    REQUIRE(static_cast<int>(ResourceCategory::labor_hours) >= 0);
}

TEST_CASE("ChemicalComposition stores key-value pairs", "[resource]") {
    ChemicalComposition comp;
    comp["N_percent"] = 1.5;
    comp["P_percent"] = 1.2;
    REQUIRE(comp["N_percent"] == 1.5);
    REQUIRE(comp["P_percent"] == 1.2);
}
