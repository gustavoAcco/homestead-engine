#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace homestead;

TEST_CASE("Area constraint exceeded emits area_exceeded diagnostic", "[solver][constraints]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{50.0}}};

    SolverConfig cfg;
    cfg.max_area_m2 = 0.001;  // tiny constraint: virtually nothing fits

    auto result = homestead::solve(goals, reg, cfg);

    bool has_area = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::area_exceeded;
    });
    REQUIRE(has_area);
}

TEST_CASE("Budget constraint exceeded emits unsatisfied_goal diagnostic",
          "[solver][constraints][budget]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{20.0}}};

    SolverConfig cfg;
    cfg.max_budget = 0.01;  // essentially zero budget

    auto result = homestead::solve(goals, reg, cfg);

    bool has_budget = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::unsatisfied_goal;
    });
    REQUIRE(has_budget);
}

TEST_CASE("Labor constraint exceeded emits unsatisfied_goal diagnostic per month",
          "[solver][constraints][labor]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"broiler_meat_kg", VariableQuantity{100.0}},
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{100.0}},
        ProductionGoal{"lettuce_head", VariableQuantity{200.0}},
    };

    SolverConfig cfg;
    cfg.max_labor_hours_per_month = 0.001;  // zero labor allowed

    auto result = homestead::solve(goals, reg, cfg);

    bool has_labor = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::unsatisfied_goal;
    });
    REQUIRE(has_labor);
}

TEST_CASE("No area diagnostic when constraint is generous", "[solver][constraints]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {ProductionGoal{"lettuce_head", VariableQuantity{5.0}}};

    SolverConfig cfg;
    cfg.max_area_m2 = 10000.0;  // very generous

    auto result = homestead::solve(goals, reg, cfg);

    bool has_area = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::area_exceeded;
    });
    REQUIRE_FALSE(has_area);
}
