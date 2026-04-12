#include <homestead/core/registry.hpp>
#include <homestead/serialization/serialization.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace homestead;

// ── Registry round-trip ───────────────────────────────────────────────────────

TEST_CASE("Registry round-trip: default 20 entities survive serialization",
          "[serialization][roundtrip]") {
    Registry original = Registry::load_defaults();
    nlohmann::json j = to_json(original);
    auto restored = registry_from_json(j);

    REQUIRE(restored.has_value());
    REQUIRE(restored->entities().size() == original.entities().size());
    REQUIRE(restored->resources().size() == original.resources().size());
}

TEST_CASE("Registry round-trip: custom entities preserved", "[serialization][roundtrip]") {
    Registry reg = Registry::load_defaults();

    // Add 3 custom resources + 1 entity
    for (int i = 1; i <= 3; ++i) {
        Resource r;
        r.slug = std::string{"custom_res_"} + std::to_string(i);
        r.name = "Custom Resource " + std::to_string(i);
        r.category = ResourceCategory::other;
        r.physical = PhysicalProperties{1.0, 0.0, -1};
        (void)reg.register_resource(r);
    }

    Entity e;
    e.slug = "custom_entity_1";
    e.name = "Custom Entity 1";
    e.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    e.outputs = {ResourceFlowSpec{"custom_res_1", VariableQuantity{1.0, 2.0, 3.0}, 1.0}};
    e.inputs = {ResourceFlowSpec{"custom_res_2", VariableQuantity{0.5}, 0.0}};
    e.infrastructure.area_m2 = VariableQuantity{1.0};
    e.infrastructure.initial_cost = VariableQuantity{50.0, 100.0, 150.0};
    (void)reg.register_entity(e);

    auto j = to_json(reg);
    auto restored = registry_from_json(j);

    REQUIRE(restored.has_value());
    auto ce = restored->find_entity("custom_entity_1");
    REQUIRE(ce.has_value());
    REQUIRE(ce->outputs.size() == 1);
    REQUIRE(ce->outputs[0].quantity_per_cycle.min == 1.0);
    REQUIRE(ce->outputs[0].quantity_per_cycle.expected == 2.0);
    REQUIRE(ce->outputs[0].quantity_per_cycle.max == 3.0);
    REQUIRE(ce->infrastructure.initial_cost.expected == 100.0);
}

// ── Graph round-trip ──────────────────────────────────────────────────────────

TEST_CASE("Graph round-trip: nodes and edges preserved", "[serialization][roundtrip]") {
    Graph g;
    EntityInstanceNode inst;
    inst.instance_name = "tank1";
    inst.entity_slug = "tilapia_tank_5000l";
    inst.quantity = VariableQuantity{1.0};
    inst.schedule = ALL_MONTHS;
    NodeId inst_id = g.add_entity_instance(inst);

    NodeId src_id = g.add_external_source(ExternalSourceNode{INVALID_NODE, "fish_feed_kg", 3.5});
    NodeId sink_id =
        g.add_goal_sink(GoalSinkNode{INVALID_NODE, "tilapia_whole_kg", VariableQuantity{20.0}});

    ResourceFlow f1;
    f1.from = src_id;
    f1.to = inst_id;
    f1.resource_slug = "fish_feed_kg";
    f1.quantity_per_cycle = VariableQuantity{35.0};
    ResourceFlow f2;
    f2.from = inst_id;
    f2.to = sink_id;
    f2.resource_slug = "tilapia_whole_kg";
    f2.quantity_per_cycle = VariableQuantity{50.0};
    (void)g.add_flow(f1);
    (void)g.add_flow(f2);

    auto j = to_json(g);
    auto restored = graph_from_json(j);

    REQUIRE(restored.has_value());
    REQUIRE(restored->node_count() == g.node_count());
}

// ── PlanResult round-trip ─────────────────────────────────────────────────────

TEST_CASE("PlanResult round-trip: balance sheet values identical after deserialize",
          "[serialization][roundtrip]") {
    Registry reg = Registry::load_defaults();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{5.0}}};
    auto original = homestead::solve(goals, reg);

    auto j = to_json(original);
    auto restored = plan_from_json(j);
    REQUIRE(restored.has_value());

    REQUIRE(restored->loop_closure_score == original.loop_closure_score);
    REQUIRE(restored->balance_sheet.size() == original.balance_sheet.size());
    REQUIRE(restored->diagnostics.size() == original.diagnostics.size());
}

// ── NutrientDemand round-trip (T024) ──────────────────────────────────────────

TEST_CASE("Entity nutrient_demand round-trip: set value preserved",
          "[serialization][roundtrip][nutrient]") {
    Registry reg;

    Resource r;
    r.slug = "crop_out_kg";
    r.name = "Crop Output";
    r.category = ResourceCategory::food_product;
    r.physical = PhysicalProperties{1.0, 0.0, 14};
    (void)reg.register_resource(r);

    Entity e;
    e.slug = "test_crop";
    e.name = "Test Crop";
    e.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    e.outputs = {ResourceFlowSpec{"crop_out_kg", VariableQuantity{1.0}, 1.0}};
    e.infrastructure.area_m2 = VariableQuantity{1.0};
    e.nutrient_demand = NutrientDemand{10.0, 4.0, 8.0};
    (void)reg.register_entity(e);

    auto j = to_json(reg);
    auto restored = registry_from_json(j);
    REQUIRE(restored.has_value());

    auto crop = restored->find_entity("test_crop");
    REQUIRE(crop.has_value());
    REQUIRE(crop->nutrient_demand.has_value());
    REQUIRE(crop->nutrient_demand->n_g_per_m2_per_cycle == 10.0);
    REQUIRE(crop->nutrient_demand->p_g_per_m2_per_cycle == 4.0);
    REQUIRE(crop->nutrient_demand->k_g_per_m2_per_cycle == 8.0);
}

TEST_CASE("Entity nutrient_demand round-trip: nullopt not serialized",
          "[serialization][roundtrip][nutrient]") {
    Registry reg;

    Resource r;
    r.slug = "out_kg";
    r.name = "Output";
    r.category = ResourceCategory::other;
    r.physical = PhysicalProperties{1.0, 0.0, -1};
    (void)reg.register_resource(r);

    Entity e;
    e.slug = "non_crop";
    e.name = "Non-crop Entity";
    e.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    e.outputs = {ResourceFlowSpec{"out_kg", VariableQuantity{1.0}, 1.0}};
    e.infrastructure.area_m2 = VariableQuantity{1.0};
    // nutrient_demand left as nullopt
    (void)reg.register_entity(e);

    auto j = to_json(reg);

    // Verify no nutrient_demand key in the serialized entity JSON.
    const auto& entities = j["data"]["entities"];
    bool found_key = false;
    for (const auto& ej : entities) {
        if (ej["slug"] == "non_crop" && ej.contains("nutrient_demand")) {
            found_key = true;
        }
    }
    REQUIRE_FALSE(found_key);

    // Also verify deserialization restores nullopt.
    auto restored = registry_from_json(j);
    REQUIRE(restored.has_value());
    auto nc = restored->find_entity("non_crop");
    REQUIRE(nc.has_value());
    REQUIRE_FALSE(nc->nutrient_demand.has_value());
}

// ── PlanResult::nutrient_balance round-trip (T025) ────────────────────────────

TEST_CASE("PlanResult nutrient_balance round-trip: non-trivial values lossless",
          "[serialization][roundtrip][nutrient]") {
    PlanResult plan;
    NutrientBalance nb;
    for (int m = 0; m < 12; ++m) {
        nb.available_n[m] = 50.0 + m * 3.7;
        nb.available_p[m] = 20.0 + m * 1.3;
        nb.available_k[m] = 40.0 + m * 2.1;
        nb.demanded_n[m] = 30.0 + m * 4.5;
        nb.demanded_p[m] = 10.0 + m * 0.9;
        nb.demanded_k[m] = 25.0 + m * 1.7;
    }
    plan.nutrient_balance = nb;

    auto j = to_json(plan);
    auto restored = plan_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->nutrient_balance.has_value());

    const auto& rnb = *restored->nutrient_balance;
    for (int m = 0; m < 12; ++m) {
        INFO("month " << m);
        REQUIRE(rnb.available_n[m] == nb.available_n[m]);
        REQUIRE(rnb.available_p[m] == nb.available_p[m]);
        REQUIRE(rnb.available_k[m] == nb.available_k[m]);
        REQUIRE(rnb.demanded_n[m] == nb.demanded_n[m]);
        REQUIRE(rnb.demanded_p[m] == nb.demanded_p[m]);
        REQUIRE(rnb.demanded_k[m] == nb.demanded_k[m]);
    }
}
