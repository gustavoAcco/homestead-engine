#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace homestead;

// ── Balance invariant ──────────────────────────────────────────────────────────

TEST_CASE("Balance invariant: internal + external >= consumption when no diagnostics",
          "[solver][properties]") {
    Registry reg = Registry::load_defaults();

    auto resource_slug = GENERATE(std::string{"broiler_meat_kg"}, std::string{"tilapia_whole_kg"},
                                  std::string{"lettuce_head"}, std::string{"egg"});

    std::vector<ProductionGoal> goals = {ProductionGoal{resource_slug, VariableQuantity{5.0}}};

    auto result = homestead::solve(goals, reg);

    // Only enforce invariant when no unsatisfied_goal / missing_producer diagnostics.
    bool unsatisfied = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::unsatisfied_goal ||
               d.kind == DiagnosticKind::missing_producer;
    });
    if (unsatisfied)
        return;

    for (const auto& bal : result.balance_sheet) {
        for (std::size_t m = 0; m < 12; ++m) {
            double supply = bal.internal_production[m] + bal.external_purchase[m];
            double demand = bal.consumption[m];
            // Allow tiny floating-point slack.
            INFO("resource=" << bal.resource_slug << " month=" << m << " supply=" << supply
                             << " demand=" << demand);
            REQUIRE(supply >= demand - 1e-9);
        }
    }
}

// ── Loop closure score bounds ──────────────────────────────────────────────────

TEST_CASE("Loop closure score is in [0.0, 1.0] for any goal", "[solver][properties]") {
    Registry reg = Registry::load_defaults();

    auto resource_slug = GENERATE(std::string{"broiler_meat_kg"}, std::string{"tilapia_whole_kg"},
                                  std::string{"egg"}, std::string{"goat_milk_l"});

    std::vector<ProductionGoal> goals = {ProductionGoal{resource_slug, VariableQuantity{10.0}}};

    auto result = homestead::solve(goals, reg);
    REQUIRE(result.loop_closure_score >= 0.0);
    REQUIRE(result.loop_closure_score <= 1.0);
}

// ── Idempotence ───────────────────────────────────────────────────────────────

TEST_CASE("Identical inputs produce identical loop_closure_score", "[solver][properties]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{10.0}}};

    auto r1 = homestead::solve(goals, reg);
    auto r2 = homestead::solve(goals, reg);

    REQUIRE(r1.loop_closure_score == r2.loop_closure_score);
    REQUIRE(r1.graph.node_count() == r2.graph.node_count());
    REQUIRE(r1.graph.edge_count() == r2.graph.edge_count());
    REQUIRE(r1.diagnostics.size() == r2.diagnostics.size());
}

// ── Solver never throws ───────────────────────────────────────────────────────

TEST_CASE("Solver never throws for empty goals", "[solver][properties]") {
    Registry reg = Registry::load_defaults();
    REQUIRE_NOTHROW(homestead::solve({}, reg));
}

TEST_CASE("Solver never throws for unknown resource goal", "[solver][properties]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {
        ProductionGoal{"completely_made_up_slug", VariableQuantity{999.0}}};
    REQUIRE_NOTHROW(homestead::solve(goals, reg));
}

TEST_CASE("Solver never throws for empty registry", "[solver][properties]") {
    Registry empty_reg;
    std::vector<ProductionGoal> goals = {ProductionGoal{"anything", VariableQuantity{1.0}}};
    REQUIRE_NOTHROW(homestead::solve(goals, empty_reg));
}
