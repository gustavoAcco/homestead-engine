#include <homestead/graph/graph.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace homestead;

// ── Helpers ────────────────────────────────────────────────────────────────────

static EntityInstanceNode make_instance(std::string name, std::string entity_slug) {
    EntityInstanceNode n;
    n.instance_name = std::move(name);
    n.entity_slug = std::move(entity_slug);
    n.quantity = VariableQuantity{1.0};
    n.schedule = ALL_MONTHS;
    return n;
}

static ExternalSourceNode make_source(std::string resource_slug) {
    ExternalSourceNode n;
    n.resource_slug = std::move(resource_slug);
    n.cost_per_unit = 0.0;
    return n;
}

static GoalSinkNode make_sink(std::string resource_slug) {
    GoalSinkNode n;
    n.resource_slug = std::move(resource_slug);
    n.quantity_per_month = VariableQuantity{10.0};
    return n;
}

// ── Construction ───────────────────────────────────────────────────────────────

TEST_CASE("Empty graph has zero nodes and edges", "[graph][construction]") {
    Graph g;
    REQUIRE(g.node_count() == 0);
    REQUIRE(g.edge_count() == 0);
}

TEST_CASE("add_entity_instance returns valid NodeId", "[graph][construction]") {
    Graph g;
    NodeId id = g.add_entity_instance(make_instance("coop1", "broiler_chicken"));
    REQUIRE(id != INVALID_NODE);
    REQUIRE(g.node_count() == 1);
}

TEST_CASE("add_external_source returns valid NodeId", "[graph][construction]") {
    Graph g;
    NodeId id = g.add_external_source(make_source("fish_feed_kg"));
    REQUIRE(id != INVALID_NODE);
    REQUIRE(g.node_count() == 1);
}

TEST_CASE("add_goal_sink returns valid NodeId", "[graph][construction]") {
    Graph g;
    NodeId id = g.add_goal_sink(make_sink("broiler_meat_kg"));
    REQUIRE(id != INVALID_NODE);
    REQUIRE(g.node_count() == 1);
}

TEST_CASE("Three nodes have distinct ids", "[graph][construction]") {
    Graph g;
    NodeId a = g.add_entity_instance(make_instance("a", "broiler_chicken"));
    NodeId b = g.add_external_source(make_source("poultry_feed_kg"));
    NodeId c = g.add_goal_sink(make_sink("broiler_meat_kg"));
    REQUIRE(a != b);
    REQUIRE(b != c);
    REQUIRE(a != c);
    REQUIRE(g.node_count() == 3);
}

// ── get_node ──────────────────────────────────────────────────────────────────

TEST_CASE("get_node returns correct variant type", "[graph][construction]") {
    Graph g;
    NodeId ei = g.add_entity_instance(make_instance("coop", "broiler_chicken"));
    NodeId es = g.add_external_source(make_source("feed_kg"));
    NodeId gs = g.add_goal_sink(make_sink("meat_kg"));

    REQUIRE(std::holds_alternative<EntityInstanceNode>(*g.get_node(ei)));
    REQUIRE(std::holds_alternative<ExternalSourceNode>(*g.get_node(es)));
    REQUIRE(std::holds_alternative<GoalSinkNode>(*g.get_node(gs)));
}

TEST_CASE("get_node returns nullopt for unknown id", "[graph][construction]") {
    Graph g;
    REQUIRE_FALSE(g.get_node(9999).has_value());
}

// ── add_flow ──────────────────────────────────────────────────────────────────

TEST_CASE("add_flow connects two nodes", "[graph][construction]") {
    Graph g;
    NodeId src = g.add_external_source(make_source("feed_kg"));
    NodeId dst = g.add_entity_instance(make_instance("coop", "broiler_chicken"));

    ResourceFlow flow;
    flow.from = src;
    flow.to = dst;
    flow.resource_slug = "feed_kg";
    flow.quantity_per_cycle = VariableQuantity{4.0};

    auto result = g.add_flow(flow);
    REQUIRE(result.has_value());
    REQUIRE(g.edge_count() == 1);
}

TEST_CASE("add_flow returns self_loop error for same from/to", "[graph][construction]") {
    Graph g;
    NodeId id = g.add_entity_instance(make_instance("coop", "broiler_chicken"));

    ResourceFlow flow;
    flow.from = id;
    flow.to = id;
    flow.resource_slug = "meat_kg";
    flow.quantity_per_cycle = VariableQuantity{1.0};

    auto result = g.add_flow(flow);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == GraphErrorKind::self_loop);
}

TEST_CASE("add_flow returns unknown_node for missing from", "[graph][construction]") {
    Graph g;
    NodeId dst = g.add_goal_sink(make_sink("meat_kg"));

    ResourceFlow flow;
    flow.from = 9999;
    flow.to = dst;
    flow.resource_slug = "meat_kg";
    flow.quantity_per_cycle = VariableQuantity{1.0};

    auto result = g.add_flow(flow);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == GraphErrorKind::unknown_node);
}

// ── successors / predecessors ─────────────────────────────────────────────────

TEST_CASE("successors and predecessors are correct after add_flow", "[graph][construction]") {
    Graph g;
    NodeId src = g.add_external_source(make_source("feed_kg"));
    NodeId mid = g.add_entity_instance(make_instance("coop", "broiler_chicken"));
    NodeId dst = g.add_goal_sink(make_sink("meat_kg"));

    ResourceFlow f1;
    f1.from = src;
    f1.to = mid;
    f1.resource_slug = "feed_kg";
    f1.quantity_per_cycle = VariableQuantity{4.0};
    ResourceFlow f2;
    f2.from = mid;
    f2.to = dst;
    f2.resource_slug = "meat_kg";
    f2.quantity_per_cycle = VariableQuantity{2.0};
    (void)g.add_flow(f1);
    (void)g.add_flow(f2);

    REQUIRE(g.successors(src).size() == 1);
    REQUIRE(g.successors(src)[0] == mid);
    REQUIRE(g.predecessors(mid).size() == 1);
    REQUIRE(g.predecessors(mid)[0] == src);
    REQUIRE(g.successors(mid).size() == 1);
    REQUIRE(g.successors(mid)[0] == dst);
    REQUIRE(g.predecessors(dst).size() == 1);
    REQUIRE(g.predecessors(dst)[0] == mid);

    // Source has no predecessors, sink has no successors
    REQUIRE(g.predecessors(src).empty());
    REQUIRE(g.successors(dst).empty());
}

// ── remove_node ───────────────────────────────────────────────────────────────

TEST_CASE("remove_node removes node and attached edges", "[graph][construction]") {
    Graph g;
    NodeId a = g.add_entity_instance(make_instance("a", "broiler_chicken"));
    NodeId b = g.add_goal_sink(make_sink("meat_kg"));

    ResourceFlow f;
    f.from = a;
    f.to = b;
    f.resource_slug = "meat_kg";
    f.quantity_per_cycle = VariableQuantity{1.0};
    (void)g.add_flow(f);
    REQUIRE(g.edge_count() == 1);

    auto result = g.remove_node(a);
    REQUIRE(result.has_value());
    REQUIRE(g.node_count() == 1);
    REQUIRE(g.edge_count() == 0);
    REQUIRE(g.predecessors(b).empty());
}

TEST_CASE("remove_node returns error for unknown id", "[graph][construction]") {
    Graph g;
    auto result = g.remove_node(42);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == GraphErrorKind::unknown_node);
}
