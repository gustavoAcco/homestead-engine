#include <homestead/core/registry.hpp>
#include <homestead/graph/graph.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace homestead;

// ── Fixtures ───────────────────────────────────────────────────────────────────

static Registry make_minimal_registry() {
    Registry reg;
    // Resources
    auto r1 = Resource{"poultry_feed_kg",      "Poultry Feed",
                       ResourceCategory::feed, {},
                       std::nullopt,           PhysicalProperties{1.0, 0.0, 90}};
    auto r2 = Resource{"broiler_meat_kg",
                       "Broiler Meat",
                       ResourceCategory::food_product,
                       {},
                       std::nullopt,
                       PhysicalProperties{1.0, 0.0, 4}};
    auto r3 = Resource{"chicken_manure_kg",
                       "Chicken Manure",
                       ResourceCategory::waste,
                       {},
                       std::nullopt,
                       PhysicalProperties{1.0, 0.0, 90}};
    (void)reg.register_resource(r1);
    (void)reg.register_resource(r2);
    (void)reg.register_resource(r3);

    // Entity
    Entity e;
    e.slug = "broiler_chicken";
    e.name = "Broiler Chicken";
    e.lifecycle = Lifecycle{7, 60, 6.0, ALL_MONTHS};
    e.inputs = {ResourceFlowSpec{"poultry_feed_kg", VariableQuantity{4.0}, 0.5}};
    e.outputs = {ResourceFlowSpec{"broiler_meat_kg", VariableQuantity{2.0}, 1.0},
                 ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{1.5}, 0.5}};
    e.infrastructure.area_m2 = VariableQuantity{0.12};
    (void)reg.register_entity(e);
    return reg;
}

// ── unsatisfied_inputs ────────────────────────────────────────────────────────

TEST_CASE("unsatisfied_inputs returns missing feed for entity with no incoming flow",
          "[graph][traversal]") {
    Registry reg = make_minimal_registry();
    Graph g;

    EntityInstanceNode inst;
    inst.instance_name = "coop1";
    inst.entity_slug = "broiler_chicken";
    inst.quantity = VariableQuantity{1.0};
    inst.schedule = ALL_MONTHS;
    NodeId inst_id = g.add_entity_instance(inst);

    auto gaps = g.unsatisfied_inputs(reg);
    REQUIRE(gaps.size() == 1);
    REQUIRE(gaps[0].first == inst_id);
    REQUIRE(gaps[0].second == "poultry_feed_kg");
}

TEST_CASE("unsatisfied_inputs empty when input is satisfied", "[graph][traversal]") {
    Registry reg = make_minimal_registry();
    Graph g;

    NodeId src = g.add_external_source(ExternalSourceNode{INVALID_NODE, "poultry_feed_kg", 2.5});

    EntityInstanceNode inst;
    inst.instance_name = "coop1";
    inst.entity_slug = "broiler_chicken";
    inst.quantity = VariableQuantity{1.0};
    inst.schedule = ALL_MONTHS;
    NodeId inst_id = g.add_entity_instance(inst);

    ResourceFlow f;
    f.from = src;
    f.to = inst_id;
    f.resource_slug = "poultry_feed_kg";
    f.quantity_per_cycle = VariableQuantity{4.0};
    (void)g.add_flow(f);

    auto gaps = g.unsatisfied_inputs(reg);
    REQUIRE(gaps.empty());
}

TEST_CASE("ExternalSourceNode has no unsatisfied inputs", "[graph][traversal]") {
    Registry reg = make_minimal_registry();
    Graph g;
    g.add_external_source(ExternalSourceNode{INVALID_NODE, "poultry_feed_kg", 2.5});

    auto gaps = g.unsatisfied_inputs(reg);
    REQUIRE(gaps.empty());
}

TEST_CASE("GoalSinkNode has no unsatisfied inputs", "[graph][traversal]") {
    Registry reg = make_minimal_registry();
    Graph g;
    g.add_goal_sink(GoalSinkNode{INVALID_NODE, "broiler_meat_kg", VariableQuantity{50.0}});

    auto gaps = g.unsatisfied_inputs(reg);
    REQUIRE(gaps.empty());
}

TEST_CASE("unsatisfied_inputs reports each missing input slug separately", "[graph][traversal]") {
    Registry reg;
    (void)reg.register_resource(Resource{"water_l",
                                         "Water",
                                         ResourceCategory::water,
                                         {},
                                         std::nullopt,
                                         PhysicalProperties{1.0, 1.0, -1}});
    (void)reg.register_resource(Resource{"feed_kg",
                                         "Feed",
                                         ResourceCategory::feed,
                                         {},
                                         std::nullopt,
                                         PhysicalProperties{1.0, 0.0, 90}});
    (void)reg.register_resource(Resource{"product_kg",
                                         "Product",
                                         ResourceCategory::food_product,
                                         {},
                                         std::nullopt,
                                         PhysicalProperties{1.0, 0.0, 3}});

    Entity e;
    e.slug = "two_input_entity";
    e.name = "Two-Input";
    e.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    e.inputs = {ResourceFlowSpec{"water_l", VariableQuantity{10.0}, 0.5},
                ResourceFlowSpec{"feed_kg", VariableQuantity{5.0}, 0.5}};
    e.outputs = {ResourceFlowSpec{"product_kg", VariableQuantity{3.0}, 1.0}};
    e.infrastructure.area_m2 = VariableQuantity{1.0};
    (void)reg.register_entity(e);

    Graph g;
    EntityInstanceNode inst;
    inst.instance_name = "inst1";
    inst.entity_slug = "two_input_entity";
    inst.quantity = VariableQuantity{1.0};
    inst.schedule = ALL_MONTHS;
    g.add_entity_instance(inst);

    auto gaps = g.unsatisfied_inputs(reg);
    REQUIRE(gaps.size() == 2);
}
