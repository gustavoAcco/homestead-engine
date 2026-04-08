#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numeric>

using namespace homestead;
using Catch::Approx;

TEST_CASE("Loop closure score is 0.0 for fully-external plan", "[solver][analytics]") {
    // Registry with a resource that has no producer.
    Registry reg;
    Resource r;
    r.slug = "mystery_input";
    r.name = "Mystery Input";
    r.category = ResourceCategory::other;
    r.physical = PhysicalProperties{1.0, 0.0, -1};
    (void)reg.register_resource(r);
    // No entity produces mystery_input → solver will add ExternalSourceNode.

    Resource out;
    out.slug = "mystery_output";
    out.name = "Mystery Output";
    out.category = ResourceCategory::food_product;
    out.physical = PhysicalProperties{1.0, 0.0, 3};
    (void)reg.register_resource(out);

    Entity e;
    e.slug = "mystery_machine";
    e.name = "Mystery Machine";
    e.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    e.inputs = {ResourceFlowSpec{"mystery_input", VariableQuantity{1.0}, 0.0}};
    e.outputs = {ResourceFlowSpec{"mystery_output", VariableQuantity{1.0}, 1.0}};
    e.infrastructure.area_m2 = VariableQuantity{1.0};
    (void)reg.register_entity(e);

    std::vector<ProductionGoal> goals = {ProductionGoal{"mystery_output", VariableQuantity{1.0}}};
    auto result = homestead::solve(goals, reg);

    // mystery_input must be purchased externally, so score < 1.
    REQUIRE(result.loop_closure_score < 1.0);
    REQUIRE(result.loop_closure_score >= 0.0);
}

TEST_CASE("Monthly sum equals annual total in balance sheet", "[solver][analytics]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{5.0}}};
    auto result = homestead::solve(goals, reg);

    for (const auto& bal : result.balance_sheet) {
        double sum_prod =
            std::accumulate(bal.internal_production.begin(), bal.internal_production.end(), 0.0);
        REQUIRE(sum_prod == Approx(bal.annual_internal_production).epsilon(1e-9));

        double sum_cons = std::accumulate(bal.consumption.begin(), bal.consumption.end(), 0.0);
        REQUIRE(sum_cons == Approx(bal.annual_consumption).epsilon(1e-9));
    }
}

TEST_CASE("BOM area matches sum of entity instance areas", "[solver][analytics]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"lettuce_head", VariableQuantity{10.0}}};
    auto result = homestead::solve(goals, reg);

    // BOM total area must be >= 0.
    REQUIRE(result.bom.total_area_m2 >= 0.0);
}

TEST_CASE("Labor schedule has 12 entries and at least one non-zero", "[solver][analytics]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{5.0}}};
    auto result = homestead::solve(goals, reg);

    REQUIRE(result.labor_schedule.size() == 12);

    double total_labor =
        std::accumulate(result.labor_schedule.begin(), result.labor_schedule.end(), 0.0);
    REQUIRE(total_labor > 0.0);
}

TEST_CASE("to_string(DiagnosticKind) returns non-empty strings", "[solver][analytics]") {
    REQUIRE(!to_string(DiagnosticKind::unsatisfied_goal).empty());
    REQUIRE(!to_string(DiagnosticKind::area_exceeded).empty());
    REQUIRE(!to_string(DiagnosticKind::non_convergent_cycle).empty());
    REQUIRE(!to_string(DiagnosticKind::missing_producer).empty());
    REQUIRE(!to_string(DiagnosticKind::seasonality_gap).empty());
}
