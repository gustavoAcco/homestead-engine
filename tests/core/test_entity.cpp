#include <homestead/core/entity.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace homestead;

TEST_CASE("Lifecycle default values", "[entity]") {
    Lifecycle lc;
    REQUIRE(lc.setup_days == 0);
    REQUIRE(lc.cycle_days == 1);
    REQUIRE(lc.cycles_per_year == 1.0);
    REQUIRE(lc.active_months == ALL_MONTHS);
}

TEST_CASE("Lifecycle constructed with custom values", "[entity]") {
    Lifecycle lc{7, 60, 6.0, 0x0FFFu};
    REQUIRE(lc.setup_days == 7);
    REQUIRE(lc.cycle_days == 60);
    REQUIRE(lc.cycles_per_year == 6.0);
    REQUIRE(lc.active_months == ALL_MONTHS);
}

TEST_CASE("ResourceFlowSpec stores slug, quantity and timing", "[entity]") {
    ResourceFlowSpec spec{"corn_grain_kg", VariableQuantity{1.0, 1.5, 2.0}, 0.8};
    REQUIRE(spec.resource_slug == "corn_grain_kg");
    REQUIRE(spec.quantity_per_cycle.expected == 1.5);
    REQUIRE(spec.timing_within_cycle == 0.8);
}

TEST_CASE("InfrastructureSpec default construction", "[entity]") {
    InfrastructureSpec infra;
    REQUIRE(infra.area_m2.expected == 0.0);
    REQUIRE(infra.construction_materials.empty());
    REQUIRE(infra.initial_labor_hours.expected == 0.0);
    REQUIRE(infra.initial_cost.expected == 0.0);
}

TEST_CASE("Entity can be constructed with required fields", "[entity]") {
    Entity e;
    e.slug = "broiler_chicken";
    e.name = "Broiler Chicken";
    e.lifecycle = Lifecycle{7, 60, 6.0, ALL_MONTHS};
    e.outputs.push_back(ResourceFlowSpec{"broiler_meat_kg", VariableQuantity{2.0}, 1.0});
    e.infrastructure.area_m2 = VariableQuantity{0.12};

    REQUIRE(e.slug == "broiler_chicken");
    REQUIRE(e.outputs.size() == 1);
    REQUIRE(e.inputs.empty());
}

TEST_CASE("Entity with no outputs can still be constructed (Registry validates)", "[entity]") {
    // Entity struct itself does not enforce the ≥1 output rule; Registry does.
    Entity e;
    e.slug = "empty_entity";
    e.name = "Empty";
    REQUIRE(e.outputs.empty());
}
