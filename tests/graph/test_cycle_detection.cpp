#include <homestead/graph/graph.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <unordered_set>

using namespace homestead;

static NodeId add_ei(Graph& g, std::string name) {
    EntityInstanceNode n;
    n.instance_name = std::move(name);
    n.entity_slug = "broiler_chicken";
    n.quantity = VariableQuantity{1.0};
    n.schedule = ALL_MONTHS;
    return g.add_entity_instance(n);
}

static void connect(Graph& g, NodeId from, NodeId to, std::string slug = "res") {
    ResourceFlow f;
    f.from = from;
    f.to = to;
    f.resource_slug = std::move(slug);
    f.quantity_per_cycle = VariableQuantity{1.0};
    (void)g.add_flow(f);
}

// ── has_cycle ─────────────────────────────────────────────────────────────────

TEST_CASE("Empty graph has no cycle", "[graph][cycle]") {
    Graph g;
    REQUIRE_FALSE(g.has_cycle());
}

TEST_CASE("Linear chain has no cycle", "[graph][cycle]") {
    Graph g;
    NodeId a = add_ei(g, "a");
    NodeId b = add_ei(g, "b");
    NodeId c = add_ei(g, "c");
    connect(g, a, b);
    connect(g, b, c);
    REQUIRE_FALSE(g.has_cycle());
}

TEST_CASE("Two-node cycle detected by has_cycle", "[graph][cycle]") {
    Graph g;
    NodeId a = add_ei(g, "a");
    NodeId b = add_ei(g, "b");
    connect(g, a, b);
    connect(g, b, a);
    REQUIRE(g.has_cycle());
}

TEST_CASE("Three-node cycle detected by has_cycle", "[graph][cycle]") {
    Graph g;
    NodeId a = add_ei(g, "a");
    NodeId b = add_ei(g, "b");
    NodeId c = add_ei(g, "c");
    connect(g, a, b);
    connect(g, b, c);
    connect(g, c, a);
    REQUIRE(g.has_cycle());
}

// ── find_cycles ───────────────────────────────────────────────────────────────

TEST_CASE("Acyclic graph find_cycles returns empty", "[graph][cycle]") {
    Graph g;
    NodeId a = add_ei(g, "a");
    NodeId b = add_ei(g, "b");
    connect(g, a, b);
    REQUIRE(g.find_cycles().empty());
}

TEST_CASE("Two-node cycle enumerated by find_cycles", "[graph][cycle]") {
    Graph g;
    NodeId a = add_ei(g, "a");
    NodeId b = add_ei(g, "b");
    connect(g, a, b);
    connect(g, b, a);

    auto cycles = g.find_cycles();
    REQUIRE_FALSE(cycles.empty());
    // There should be at least one cycle of length 2
    bool found_two = std::ranges::any_of(cycles, [](const auto& c) { return c.size() == 2; });
    REQUIRE(found_two);
}

TEST_CASE("Canonical Sisteminha cycle enumerated", "[graph][cycle]") {
    // chicken_manure → compost_bin → mature_compost → corn_plot → corn_grain
    // → broiler_chicken (feed) → chicken_manure
    Graph g;
    NodeId chicken = add_ei(g, "chicken");
    NodeId compost = add_ei(g, "compost");
    NodeId corn = add_ei(g, "corn");

    // Create a 3-node cycle: chicken → compost → corn → chicken
    connect(g, chicken, compost, "chicken_manure_kg");
    connect(g, compost, corn, "mature_compost_kg");
    connect(g, corn, chicken, "corn_grain_kg");

    REQUIRE(g.has_cycle());

    auto cycles = g.find_cycles();
    REQUIRE_FALSE(cycles.empty());

    // Verify the 3-cycle is present
    bool found_three = std::ranges::any_of(cycles, [](const auto& c) { return c.size() == 3; });
    REQUIRE(found_three);

    // Every node in the 3-cycle should be one of our three nodes
    for (const auto& cycle : cycles) {
        if (cycle.size() == 3) {
            std::unordered_set<NodeId> cycle_set(cycle.begin(), cycle.end());
            REQUIRE(cycle_set.count(chicken));
            REQUIRE(cycle_set.count(compost));
            REQUIRE(cycle_set.count(corn));
        }
    }
}
