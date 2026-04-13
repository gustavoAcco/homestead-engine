#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace homestead;

TEST_CASE("Solver with empty goals returns empty PlanResult", "[solver][backprop]") {
    Registry reg = Registry::load_defaults();
    SolverConfig cfg;
    auto result = homestead::solve({}, reg, cfg);
    REQUIRE(result.graph.node_count() == 0);
    REQUIRE(result.diagnostics.empty());
}

TEST_CASE("Single goal: tilapia_whole_kg produces a plan with entity instance",
          "[solver][backprop]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{20.0}}};

    SolverConfig cfg;
    auto result = homestead::solve(goals, reg, cfg);

    // Graph must contain at least one node.
    REQUIRE(result.graph.node_count() > 0);

    // Find the tilapia tank entity instance.
    bool has_tilapia = false;
    result.graph.for_each_node([&](NodeId /*id*/, const NodeVariant& v) {
        if (std::holds_alternative<EntityInstanceNode>(v)) {
            const auto& inst = std::get<EntityInstanceNode>(v);
            if (inst.entity_slug == "tilapia_tank_5000l") {
                has_tilapia = true;
            }
        }
    });
    REQUIRE(has_tilapia);
}

TEST_CASE("Plan contains at least one balance sheet entry for tilapia goal", "[solver][backprop]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{20.0}}};

    auto result = homestead::solve(goals, reg);

    auto it = std::ranges::find_if(result.balance_sheet, [](const ResourceBalance& b) {
        return b.resource_slug == "tilapia_whole_kg";
    });
    REQUIRE(it != result.balance_sheet.end());
}

TEST_CASE("Loop closure score is in [0, 1]", "[solver][backprop]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{10.0}}};
    auto result = homestead::solve(goals, reg);
    REQUIRE(result.loop_closure_score >= 0.0);
    REQUIRE(result.loop_closure_score <= 1.0);
}

TEST_CASE("Solver never throws for goal with unknown resource", "[solver][backprop]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {
        ProductionGoal{"nonexistent_resource", VariableQuantity{5.0}}};
    // Must not throw; should produce a diagnostic.
    PlanResult result;
    REQUIRE_NOTHROW(result = homestead::solve(goals, reg));
    bool has_diagnostic = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::missing_producer;
    });
    REQUIRE(has_diagnostic);
}
