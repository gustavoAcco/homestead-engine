#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace homestead;

// Build a registry where "seasonal_veg_kg" is only produced in summer
// (months 5–7: Jun, Jul, Aug = bits 5,6,7 → 0x00E0).
static constexpr MonthMask SUMMER_ONLY = 0x00E0u;  // Jun=bit5, Jul=bit6, Aug=bit7

static Registry make_seasonal_registry() {
    Registry reg;
    Resource r;
    r.slug = "seasonal_veg_kg";
    r.name = "Seasonal Veg";
    r.category = ResourceCategory::food_product;
    r.physical = PhysicalProperties{1.0, 0.0, 3};
    (void)reg.register_resource(r);

    Resource water;
    water.slug = "fresh_water_l";
    water.name = "Water";
    water.category = ResourceCategory::water;
    water.physical = PhysicalProperties{1.0, 1.0, -1};
    (void)reg.register_resource(water);

    Entity e;
    e.slug = "seasonal_bed";
    e.name = "Seasonal Vegetable Bed";
    e.lifecycle = Lifecycle{0, 30, 3.0, SUMMER_ONLY};  // active Jun–Aug only
    e.inputs = {ResourceFlowSpec{"fresh_water_l", VariableQuantity{20.0}, 0.5}};
    e.outputs = {ResourceFlowSpec{"seasonal_veg_kg", VariableQuantity{5.0}, 1.0}};
    e.infrastructure.area_m2 = VariableQuantity{1.0};
    (void)reg.register_entity(e);

    return reg;
}

TEST_CASE("Solver emits seasonality_gap when goal month has no active entity",
          "[solver][scheduling]") {
    Registry reg = make_seasonal_registry();

    // Request seasonal_veg_kg year-round — winter months won't be covered.
    std::vector<ProductionGoal> goals = {ProductionGoal{"seasonal_veg_kg", VariableQuantity{3.0}}};

    auto result = homestead::solve(goals, reg);

    bool has_seasonality_gap = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::seasonality_gap;
    });
    REQUIRE(has_seasonality_gap);
}

TEST_CASE("No seasonality_gap when entity is active all year", "[solver][scheduling]") {
    Registry reg = Registry::load_defaults();  // all entities use ALL_MONTHS

    std::vector<ProductionGoal> goals = {ProductionGoal{"tilapia_whole_kg", VariableQuantity{5.0}}};

    auto result = homestead::solve(goals, reg);

    bool has_seasonality_gap = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::seasonality_gap;
    });
    REQUIRE_FALSE(has_seasonality_gap);
}
