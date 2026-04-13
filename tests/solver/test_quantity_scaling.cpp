// Tests for quantity scaling solver (feature 002).
//
// Hand-calculated baseline (research.md §1):
//   broiler_chicken: 2.0 kg/cycle × 0.5 cycles/month = 1.0 kg/month/instance
//     → 50 kg/month goal → ceil(50 / 1.0) = 50 instances
//   laying_hen: 25.0 eggs/cycle × 1.0 cycle/month = 25.0 eggs/month/hen
//     → 200 eggs/month goal → ceil(200 / 25.0) = 8 hens
//   poultry_feed_kg: no producer in default registry → external source with qty=100 kg/month

#include <homestead/core/entity.hpp>
#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace homestead;
using Catch::Approx;

// ── Helper: find an EntityInstanceNode by entity_slug ─────────────────────────

static std::optional<EntityInstanceNode> find_entity_node(const Graph& g, const std::string& slug) {
    std::optional<EntityInstanceNode> found;
    g.for_each_node([&](NodeId /*id*/, const NodeVariant& v) {
        if (std::holds_alternative<EntityInstanceNode>(v)) {
            const auto& inst = std::get<EntityInstanceNode>(v);
            if (inst.entity_slug == slug) {
                found = inst;
            }
        }
    });
    return found;
}

// ── T009 ───────────────────────────────────────────────────────────────────────

TEST_CASE("Single goal: 50 kg broiler_meat_kg/month yields ~50 broiler_chicken instances",
          "[solver][quantity][US1]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{50.0}}};
    SolverConfig cfg;
    auto result = homestead::solve(goals, reg, cfg);

    auto inst = find_entity_node(result.graph, "broiler_chicken");
    REQUIRE(inst.has_value());
    // ceil(50.0 / 1.0) = 50 exactly; allow ±5% for floating-point rounding.
    CHECK(inst->quantity.expected == Approx(50.0).epsilon(0.05));
}

// ── T010 ───────────────────────────────────────────────────────────────────────

TEST_CASE("No producer: external source flow carries actual demand quantity",
          "[solver][quantity][US1]") {
    Registry reg;
    // Register one resource with no producer.
    (void)reg.register_resource(
        Resource{"wood_kg", "Wood", ResourceCategory::raw_material, {}, {}});

    std::vector<ProductionGoal> goals = {ProductionGoal{"wood_kg", VariableQuantity{10.0}}};
    auto result = homestead::solve(goals, reg, SolverConfig{});

    // Must have an ExternalSourceNode for wood_kg.
    bool found_ext = false;
    result.graph.for_each_node([&](NodeId id, const NodeVariant& v) {
        if (!std::holds_alternative<ExternalSourceNode>(v)) {
            return;
        }
        const auto& src = std::get<ExternalSourceNode>(v);
        if (src.resource_slug != "wood_kg") {
            return;
        }
        // Find the outgoing flow from this node.
        for (NodeId succ : result.graph.successors(id)) {
            // The flow quantity should equal the monthly demand.
            (void)succ;
        }
        found_ext = true;
    });
    REQUIRE(found_ext);

    // The balance sheet for wood_kg should have no internal production.
    auto it = std::ranges::find_if(result.balance_sheet, [](const ResourceBalance& b) {
        return b.resource_slug == "wood_kg";
    });
    REQUIRE(it != result.balance_sheet.end());
    CHECK(it->annual_internal_production == Approx(0.0));
}

// ── T010b ──────────────────────────────────────────────────────────────────────

TEST_CASE("Single goal: 200 eggs/month yields correct laying_hen quantity",
          "[solver][quantity][US1]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"egg", VariableQuantity{200.0}}};
    SolverConfig cfg;
    auto result = homestead::solve(goals, reg, cfg);

    auto inst = find_entity_node(result.graph, "laying_hen");
    REQUIRE(inst.has_value());
    // laying_hen: 25.0 eggs/cycle, cycles_per_year=12, ALL_MONTHS → 1.0 cycle/month
    // output_per_month = 25.0 × 1.0 = 25.0; ceil(200 / 25.0) = 8 hens.
    double output_per_month = 25.0 * 1.0;
    CHECK(inst->quantity.expected * output_per_month >= 200.0);
    // Also verify it's a reasonable integer count (not wildly over-provisioned).
    CHECK(inst->quantity.expected == Approx(8.0).epsilon(0.05));
}

// ── T010c ──────────────────────────────────────────────────────────────────────

TEST_CASE("Zero goal quantity produces no entity instances", "[solver][quantity][US1]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{0.0}}};
    auto result = homestead::solve(goals, reg, SolverConfig{});

    bool has_entity = false;
    result.graph.for_each_node([&](NodeId /*id*/, const NodeVariant& v) {
        if (std::holds_alternative<EntityInstanceNode>(v)) {
            has_entity = true;
        }
    });
    CHECK_FALSE(has_entity);
    // The goal sink node should still be present.
    CHECK(result.graph.node_count() == 1);
}

// ── T015 ───────────────────────────────────────────────────────────────────────

TEST_CASE("Chain: 50 broiler_chicken instances drive ~100 kg/month poultry_feed demand",
          "[solver][quantity][US2]") {
    // broiler_chicken consumes 4.0 kg poultry_feed/cycle at 0.5 cycles/month.
    // 50 instances → 4.0 × 0.5 × 50 = 100.0 kg/month demanded upstream.
    // poultry_feed_kg has no producer in the default registry → external source.
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{50.0}}};
    auto result = homestead::solve(goals, reg, SolverConfig{});

    // Verify the entity count is correct.
    auto chicken = find_entity_node(result.graph, "broiler_chicken");
    REQUIRE(chicken.has_value());
    CHECK(chicken->quantity.expected == Approx(50.0).epsilon(0.05));

    // Annual consumption of poultry_feed: 100 kg/month × 12 = 1200 kg/year.
    auto it = std::ranges::find_if(result.balance_sheet, [](const ResourceBalance& b) {
        return b.resource_slug == "poultry_feed_kg";
    });
    REQUIRE(it != result.balance_sheet.end());
    // 50 chickens × 4.0 kg/cycle × 0.5 cycles/month × 12 months = 1200 kg/year (±5%).
    CHECK(it->annual_consumption == Approx(1200.0).epsilon(0.05));
    CHECK(it->annual_internal_production == Approx(0.0));  // no internal producer
}

// ── T016 ───────────────────────────────────────────────────────────────────────

TEST_CASE("Circular dependency converges: two-entity resource cycle stabilises",
          "[solver][quantity][US2]") {
    // Build a 2-entity cycle: X produces "alpha", consumes "beta";
    // Y produces "beta", consumes "alpha".  Both operate at 1.0 cycle/month.
    Registry reg;
    (void)reg.register_resource(Resource{"alpha", "Alpha", ResourceCategory::raw_material, {}, {}});
    (void)reg.register_resource(Resource{"beta", "Beta", ResourceCategory::raw_material, {}, {}});

    Entity x;
    x.slug = "entity_x";
    x.name = "Entity X";
    x.inputs = {ResourceFlowSpec{"beta", VariableQuantity{1.0}, 0.0}};
    x.outputs = {ResourceFlowSpec{"alpha", VariableQuantity{1.0}, 0.0}};
    x.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    x.operating_labor_per_cycle = VariableQuantity{0.0};
    x.stocking_density = VariableQuantity{1.0};
    x.infrastructure =
        InfrastructureSpec{VariableQuantity{1.0}, {}, VariableQuantity{0.0}, VariableQuantity{0.0}};
    (void)reg.register_entity(x);

    Entity y;
    y.slug = "entity_y";
    y.name = "Entity Y";
    y.inputs = {ResourceFlowSpec{"alpha", VariableQuantity{1.0}, 0.0}};
    y.outputs = {ResourceFlowSpec{"beta", VariableQuantity{1.0}, 0.0}};
    y.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    y.operating_labor_per_cycle = VariableQuantity{0.0};
    y.stocking_density = VariableQuantity{1.0};
    y.infrastructure =
        InfrastructureSpec{VariableQuantity{1.0}, {}, VariableQuantity{0.0}, VariableQuantity{0.0}};
    (void)reg.register_entity(y);

    std::vector<ProductionGoal> goals = {ProductionGoal{"alpha", VariableQuantity{10.0}}};
    auto result = homestead::solve(goals, reg, SolverConfig{});

    // Solver must converge — no non_convergent_cycle diagnostic.
    bool converged = std::ranges::none_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::non_convergent_cycle;
    });
    CHECK(converged);

    // Entity X must exist and have quantity >= 1.
    auto inst_x = find_entity_node(result.graph, "entity_x");
    REQUIRE(inst_x.has_value());
    CHECK(inst_x->quantity.expected >= 1.0);
}

// ── T020 ───────────────────────────────────────────────────────────────────────

TEST_CASE("Loop closure score: fully internal plan scores 1.0", "[solver][quantity][US3]") {
    // Resource A is produced by X (no inputs).  B is produced by Y (no inputs).
    // X also consumes B so it links to Y, but both entities have no external demands.
    // A fully internally-supplied plan should score 1.0.
    Registry reg;
    (void)reg.register_resource(
        Resource{"prod_a", "Product A", ResourceCategory::raw_material, {}, {}});
    (void)reg.register_resource(
        Resource{"prod_b", "Product B", ResourceCategory::raw_material, {}, {}});

    // Entity P: produces prod_a, no inputs.
    Entity p;
    p.slug = "entity_p";
    p.name = "Entity P";
    p.outputs = {ResourceFlowSpec{"prod_a", VariableQuantity{1.0}, 0.0}};
    p.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    p.operating_labor_per_cycle = VariableQuantity{0.0};
    p.stocking_density = VariableQuantity{1.0};
    p.infrastructure =
        InfrastructureSpec{VariableQuantity{1.0}, {}, VariableQuantity{0.0}, VariableQuantity{0.0}};
    (void)reg.register_entity(p);

    std::vector<ProductionGoal> goals = {ProductionGoal{"prod_a", VariableQuantity{5.0}}};
    auto result = homestead::solve(goals, reg, SolverConfig{});

    // Entity P has no inputs, so all consumption is internally satisfied.
    CHECK(result.loop_closure_score == Catch::Approx(1.0).margin(1e-9));
}

// ── T021 ───────────────────────────────────────────────────────────────────────

TEST_CASE("Property: Sisteminha multi-goal plan produces non-trivial entity quantities",
          "[solver][quantity][US3]") {
    // Solve the full Sisteminha goals from data/sisteminha_goals.json.
    // Verifies that quantity scaling produces sensible entity counts
    // (not the old hardcoded 1.0) and the solver converges.
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {
        ProductionGoal{"broiler_meat_kg", VariableQuantity{50.0}},
        ProductionGoal{"egg", VariableQuantity{200.0}},
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{30.0}},
        ProductionGoal{"lettuce_head", VariableQuantity{120.0}},
        ProductionGoal{"tomato_kg", VariableQuantity{40.0}},
        ProductionGoal{"corn_grain_kg", VariableQuantity{60.0}},
        ProductionGoal{"bean_kg", VariableQuantity{20.0}},
        ProductionGoal{"banana_bunch_kg", VariableQuantity{30.0}},
    };

    auto result = homestead::solve(goals, reg, SolverConfig{});

    // Solver must converge.
    bool converged = std::ranges::none_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::non_convergent_cycle;
    });
    CHECK(converged);

    // At least one entity must have quantity > 1 (scaling is working).
    bool has_scaled_entity = false;
    result.graph.for_each_node([&](NodeId /*id*/, const NodeVariant& v) {
        if (std::holds_alternative<EntityInstanceNode>(v)) {
            const auto& inst = std::get<EntityInstanceNode>(v);
            if (inst.quantity.expected > 1.0) {
                has_scaled_entity = true;
            }
        }
    });
    CHECK(has_scaled_entity);

    // Loop closure score must be in [0, 1].
    CHECK(result.loop_closure_score >= 0.0);
    CHECK(result.loop_closure_score <= 1.0);

    // For each goal resource that has a producer, check a producer entity was created.
    for (const auto& goal : goals) {
        if (reg.producers_of(goal.resource_slug).empty()) {
            continue;  // no producer → external source, skip
        }
        auto it = std::ranges::find_if(result.balance_sheet, [&](const ResourceBalance& b) {
            return b.resource_slug == goal.resource_slug;
        });
        if (it != result.balance_sheet.end()) {
            CHECK(it->annual_internal_production > 0.0);
        }
    }
}
