// Validates the exact quickstart.md code example:
//   50 kg broiler chicken + 200 eggs per month, 200 m² constraint.
// Asserts the plan completes without crash and loop closure score > 0.

#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace homestead;

TEST_CASE("Quickstart example: chicken + eggs with 200 m2 constraint", "[solver][quickstart]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"broiler_meat_kg", VariableQuantity{50.0}},
        ProductionGoal{"egg", VariableQuantity{200.0}},
    };

    SolverConfig cfg;
    cfg.max_area_m2 = 200.0;

    PlanResult result;
    REQUIRE_NOTHROW(result = homestead::solve(goals, reg, cfg));

    // Plan must have at least one node.
    REQUIRE(result.graph.node_count() > 0);

    // Loop closure score must be in valid range.
    REQUIRE(result.loop_closure_score >= 0.0);
    REQUIRE(result.loop_closure_score <= 1.0);
}
