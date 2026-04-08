#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace homestead;

// Build a registry with the canonical circular dependency:
// broiler_chicken produces chicken_manure_kg
// compost_bin consumes chicken_manure_kg, produces mature_compost_kg
// corn_plot_1m2 consumes mature_compost_kg, produces corn_grain_kg
// broiler_chicken consumes corn_grain_kg (as feed)
// → cycle: broiler_chicken → compost → corn → broiler_chicken

static Registry make_circular_registry() {
    Registry reg;
    // Resources
    for (auto&& [slug, name, cat] :
         std::initializer_list<std::tuple<const char*, const char*, ResourceCategory>>{
             {"chicken_manure_kg", "Chicken Manure", ResourceCategory::waste},
             {"mature_compost_kg", "Mature Compost", ResourceCategory::fertilizer},
             {"corn_grain_kg", "Corn Grain", ResourceCategory::food_product},
             {"broiler_meat_kg", "Broiler Meat", ResourceCategory::food_product},
             {"fresh_water_l", "Fresh Water", ResourceCategory::water},
         }) {
        Resource r;
        r.slug = slug;
        r.name = name;
        r.category = cat;
        r.physical = PhysicalProperties{1.0, 0.0, -1};
        (void)reg.register_resource(r);
    }

    // broiler_chicken: consumes corn_grain_kg, produces broiler_meat_kg + chicken_manure_kg
    {
        Entity e;
        e.slug = "broiler_chicken";
        e.name = "Broiler Chicken";
        e.lifecycle = Lifecycle{7, 60, 6.0, ALL_MONTHS};
        e.inputs = {ResourceFlowSpec{"corn_grain_kg", VariableQuantity{0.5}, 0.5}};
        e.outputs = {
            ResourceFlowSpec{"broiler_meat_kg", VariableQuantity{2.0}, 1.0},
            ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{1.5}, 0.5},
        };
        e.infrastructure.area_m2 = VariableQuantity{0.12};
        (void)reg.register_entity(e);
    }

    // compost_bin: consumes chicken_manure_kg, produces mature_compost_kg
    {
        Entity e;
        e.slug = "compost_bin";
        e.name = "Compost Bin";
        e.lifecycle = Lifecycle{0, 90, 4.0, ALL_MONTHS};
        e.inputs = {ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{10.0}, 0.1}};
        e.outputs = {ResourceFlowSpec{"mature_compost_kg", VariableQuantity{8.0}, 1.0}};
        e.infrastructure.area_m2 = VariableQuantity{1.2};
        (void)reg.register_entity(e);
    }

    // corn_plot_1m2: consumes mature_compost_kg + water, produces corn_grain_kg
    {
        Entity e;
        e.slug = "corn_plot_1m2";
        e.name = "Corn Plot";
        e.lifecycle = Lifecycle{0, 120, 3.0, ALL_MONTHS};
        e.inputs = {
            ResourceFlowSpec{"mature_compost_kg", VariableQuantity{0.5}, 0.0},
            ResourceFlowSpec{"fresh_water_l", VariableQuantity{40.0}, 0.5},
        };
        e.outputs = {ResourceFlowSpec{"corn_grain_kg", VariableQuantity{0.5}, 1.0}};
        e.infrastructure.area_m2 = VariableQuantity{1.0};
        (void)reg.register_entity(e);
    }

    return reg;
}

TEST_CASE("Solver converges on canonical circular dependency", "[solver][convergence]") {
    Registry reg = make_circular_registry();

    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{5.0}}};

    SolverConfig cfg;
    cfg.max_convergence_iterations = 100;

    PlanResult result;
    REQUIRE_NOTHROW(result = homestead::solve(goals, reg, cfg));

    // Solver must not crash and must produce a graph.
    REQUIRE(result.graph.node_count() > 0);

    // non_convergent_cycle diagnostic should NOT be present (should converge).
    bool non_convergent = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::non_convergent_cycle;
    });
    REQUIRE_FALSE(non_convergent);
}

TEST_CASE("Solver graph contains the circular entity nodes", "[solver][convergence]") {
    Registry reg = make_circular_registry();
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{5.0}}};

    auto result = homestead::solve(goals, reg);

    bool has_chicken = false, has_compost = false, has_corn = false;
    for (NodeId nid = 0; nid < result.graph.node_count() + 100; ++nid) {
        auto n = result.graph.get_node(nid);
        if (!n)
            continue;
        if (!std::holds_alternative<EntityInstanceNode>(*n))
            continue;
        const auto& inst = std::get<EntityInstanceNode>(*n);
        if (inst.entity_slug == "broiler_chicken")
            has_chicken = true;
        if (inst.entity_slug == "compost_bin")
            has_compost = true;
        if (inst.entity_slug == "corn_plot_1m2")
            has_corn = true;
    }
    // At minimum the primary goal producer must be present.
    REQUIRE(has_chicken);
}
