// Composition-aware resource matching tests (Feature 004).
// Scenarios 1-5 correspond to quickstart.md examples.
// All TEST_CASE names are pure ASCII (required for Windows CTest filter compat).

#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace homestead;
using Catch::Approx;

// ── Shared fixture helpers ────────────────────────────────────────────────────

namespace {

/// Build a minimal registry for animal-feed tests.
/// Registers: corn_grain_kg, bean_kg, broiler_meat_kg resources; corn_plot_1m2,
/// bean_plot_1m2, broiler_chicken entities. broiler_chicken declares
/// composition_requirements instead of a slug-based poultry_feed_kg input.
Registry make_feed_registry() {
    Registry reg;

    // Resources
    auto add_res = [&](std::string slug, std::string name, ResourceCategory cat) {
        Resource r;
        r.slug = std::move(slug);
        r.name = std::move(name);
        r.category = cat;
        r.physical = PhysicalProperties{1.0, 0.0, -1};
        (void)reg.register_resource(r);
    };

    add_res("corn_grain_kg", "Corn Grain", ResourceCategory::feed);
    add_res("bean_kg", "Common Bean", ResourceCategory::feed);
    add_res("corn_stover_kg", "Corn Stover", ResourceCategory::other);
    add_res("bean_straw_kg", "Bean Straw", ResourceCategory::other);
    add_res("broiler_meat_kg", "Broiler Meat", ResourceCategory::food_product);
    add_res("chicken_manure_kg", "Chicken Manure", ResourceCategory::fertilizer);
    add_res("fresh_water_l", "Fresh Water", ResourceCategory::water);
    add_res("human_labor_hours", "Human Labor", ResourceCategory::labor_hours);

    // Feed resource composition (protein_g and energy_kcal per kg)
    {
        Resource r;
        r.slug = "corn_grain_kg";
        r.name = "Corn Grain";
        r.category = ResourceCategory::feed;
        r.physical = PhysicalProperties{1.0, 0.0, -1};
        r.composition["protein_g"] = 93.0;
        r.composition["energy_kcal"] = 3580.0;
        (void)reg.register_resource(r);  // override
    }
    {
        Resource r;
        r.slug = "bean_kg";
        r.name = "Common Bean";
        r.category = ResourceCategory::feed;
        r.physical = PhysicalProperties{1.0, 0.0, -1};
        r.composition["protein_g"] = 220.0;
        r.composition["energy_kcal"] = 3400.0;
        (void)reg.register_resource(r);  // override
    }

    // corn_plot_1m2
    {
        Entity e;
        e.slug = "corn_plot_1m2";
        e.name = "Corn Plot (1 m2)";
        e.lifecycle = Lifecycle{0, 120, 3.0, ALL_MONTHS};
        e.outputs = {ResourceFlowSpec{"corn_grain_kg", VariableQuantity{0.3, 0.5, 0.7}, 1.0},
                     ResourceFlowSpec{"corn_stover_kg", VariableQuantity{0.5, 0.8, 1.0}, 1.0}};
        e.inputs = {ResourceFlowSpec{"fresh_water_l", VariableQuantity{30.0, 40.0, 50.0}, 0.5},
                    ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.3, 0.5, 0.7}, 0.5}};
        e.infrastructure.area_m2 = VariableQuantity{1.0};
        e.infrastructure.initial_cost = VariableQuantity{5.0};
        (void)reg.register_entity(e);
    }

    // bean_plot_1m2
    {
        Entity e;
        e.slug = "bean_plot_1m2";
        e.name = "Common Bean Plot (1 m2)";
        e.lifecycle = Lifecycle{0, 90, 4.0, ALL_MONTHS};
        e.outputs = {ResourceFlowSpec{"bean_kg", VariableQuantity{0.15, 0.25, 0.35}, 1.0},
                     ResourceFlowSpec{"bean_straw_kg", VariableQuantity{0.3, 0.5, 0.7}, 1.0}};
        e.inputs = {ResourceFlowSpec{"fresh_water_l", VariableQuantity{20.0, 30.0, 40.0}, 0.5},
                    ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.2, 0.3, 0.5}, 0.5}};
        e.infrastructure.area_m2 = VariableQuantity{1.0};
        e.infrastructure.initial_cost = VariableQuantity{3.0};
        (void)reg.register_entity(e);
    }

    // broiler_chicken — composition_requirements instead of poultry_feed_kg
    {
        Entity e;
        e.slug = "broiler_chicken";
        e.name = "Broiler Chicken Batch";
        e.lifecycle = Lifecycle{7, 60, 6.0, ALL_MONTHS};
        e.outputs = {ResourceFlowSpec{"broiler_meat_kg", VariableQuantity{1.5, 2.0, 2.5}, 1.0},
                     ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{1.0, 1.5, 2.0}, 0.5}};
        e.inputs = {ResourceFlowSpec{"fresh_water_l", VariableQuantity{8.0, 10.0, 12.0}, 0.5},
                    ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.25, 0.3, 0.4}, 0.5}};
        e.infrastructure.area_m2 = VariableQuantity{0.12};
        e.infrastructure.initial_cost = VariableQuantity{80.0};
        // composition_requirements: small values so 1 corn plot surplus satisfies 1 batch/year
        e.composition_requirements["protein_g"] = 4.65;
        e.composition_requirements["energy_kcal"] = 100.0;
        (void)reg.register_entity(e);
    }

    return reg;
}

/// Build a minimal registry for fertilization tests.
Registry make_fert_registry() {
    Registry reg;

    auto add_res = [&](std::string slug, std::string name, ResourceCategory cat) {
        Resource r;
        r.slug = std::move(slug);
        r.name = std::move(name);
        r.category = cat;
        r.physical = PhysicalProperties{1.0, 0.0, -1};
        (void)reg.register_resource(r);
    };

    add_res("broiler_meat_kg", "Broiler Meat", ResourceCategory::food_product);
    add_res("corn_grain_kg", "Corn Grain", ResourceCategory::feed);
    add_res("corn_stover_kg", "Corn Stover", ResourceCategory::other);
    add_res("fresh_water_l", "Fresh Water", ResourceCategory::water);
    add_res("human_labor_hours", "Human Labor", ResourceCategory::labor_hours);
    add_res("tilapia_whole_kg", "Tilapia", ResourceCategory::food_product);
    add_res("lettuce_head", "Lettuce Head", ResourceCategory::food_product);

    // chicken_manure_kg with NPK composition (g per kg)
    {
        Resource r;
        r.slug = "chicken_manure_kg";
        r.name = "Chicken Manure";
        r.category = ResourceCategory::fertilizer;
        r.physical = PhysicalProperties{1.0, 0.0, -1};
        r.composition["N_g"] = 15.0;
        r.composition["P_g"] = 12.0;
        r.composition["K_g"] = 8.0;
        (void)reg.register_resource(r);
    }

    // nutrient_water_l with N and P only (no K — per Feature 008 values)
    {
        Resource r;
        r.slug = "nutrient_water_l";
        r.name = "Nutrient Water";
        r.category = ResourceCategory::water;
        r.physical = PhysicalProperties{1.0, 1.0, -1};
        r.composition["N_g"] = 0.036;
        r.composition["P_g"] = 0.0096;
        (void)reg.register_resource(r);
    }

    // broiler_chicken (composition_requirements, produces manure)
    {
        Entity e;
        e.slug = "broiler_chicken";
        e.name = "Broiler Chicken Batch";
        e.lifecycle = Lifecycle{7, 60, 6.0, ALL_MONTHS};
        e.outputs = {ResourceFlowSpec{"broiler_meat_kg", VariableQuantity{1.5, 2.0, 2.5}, 1.0},
                     ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{1.0, 1.5, 2.0}, 0.5}};
        e.inputs = {ResourceFlowSpec{"fresh_water_l", VariableQuantity{8.0, 10.0, 12.0}, 0.5},
                    ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.25, 0.3, 0.4}, 0.5}};
        e.infrastructure.area_m2 = VariableQuantity{0.12};
        e.infrastructure.initial_cost = VariableQuantity{80.0};
        e.composition_requirements["protein_g"] = 720.0;
        e.composition_requirements["energy_kcal"] = 11600.0;
        (void)reg.register_entity(e);
    }

    // corn_plot_1m2 with fertilization_per_m2
    {
        Entity e;
        e.slug = "corn_plot_1m2";
        e.name = "Corn Plot (1 m2)";
        e.lifecycle = Lifecycle{0, 120, 3.0, ALL_MONTHS};
        e.outputs = {ResourceFlowSpec{"corn_grain_kg", VariableQuantity{0.3, 0.5, 0.7}, 1.0},
                     ResourceFlowSpec{"corn_stover_kg", VariableQuantity{0.5, 0.8, 1.0}, 1.0}};
        e.inputs = {ResourceFlowSpec{"fresh_water_l", VariableQuantity{30.0, 40.0, 50.0}, 0.5},
                    ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.3, 0.5, 0.7}, 0.5}};
        e.infrastructure.area_m2 = VariableQuantity{1.0};
        e.infrastructure.initial_cost = VariableQuantity{5.0};
        e.fertilization_per_m2["N_g"] = 8.0;
        e.fertilization_per_m2["P_g"] = 3.0;
        e.fertilization_per_m2["K_g"] = 6.0;
        (void)reg.register_entity(e);
    }

    // tilapia_tank_5000l (produces nutrient_water_l)
    {
        Resource r;
        r.slug = "tilapia_fingerlings";
        r.name = "Tilapia Fingerlings";
        r.category = ResourceCategory::other;
        r.physical = PhysicalProperties{0.01, 0.0, -1};
        (void)reg.register_resource(r);

        Resource rff;
        rff.slug = "fish_feed_kg";
        rff.name = "Fish Feed";
        rff.category = ResourceCategory::feed;
        rff.physical = PhysicalProperties{1.0, 0.0, -1};
        (void)reg.register_resource(rff);

        Entity e;
        e.slug = "tilapia_tank_5000l";
        e.name = "Tilapia Tank (5000L)";
        e.lifecycle = Lifecycle{7, 120, 3.0, ALL_MONTHS};
        e.outputs = {
            ResourceFlowSpec{"tilapia_whole_kg", VariableQuantity{40.0, 50.0, 60.0}, 1.0},
            ResourceFlowSpec{"nutrient_water_l", VariableQuantity{4500.0, 4800.0, 5000.0}, 1.0}};
        e.inputs = {
            ResourceFlowSpec{"fish_feed_kg", VariableQuantity{30.0, 35.0, 40.0}, 0.5},
            ResourceFlowSpec{"fresh_water_l", VariableQuantity{200.0, 250.0, 300.0}, 0.3},
            ResourceFlowSpec{"tilapia_fingerlings", VariableQuantity{150.0, 200.0, 250.0}, 0.0},
            ResourceFlowSpec{"human_labor_hours", VariableQuantity{2.0, 3.0, 4.0}, 0.5}};
        e.infrastructure.area_m2 = VariableQuantity{7.0};
        e.infrastructure.initial_cost = VariableQuantity{800.0};
        (void)reg.register_entity(e);
    }

    // lettuce_bed_1m2 with fertilization_per_m2
    {
        Entity e;
        e.slug = "lettuce_bed_1m2";
        e.name = "Lettuce Bed (1 m2)";
        e.lifecycle = Lifecycle{0, 45, 8.0, ALL_MONTHS};
        e.outputs = {ResourceFlowSpec{"lettuce_head", VariableQuantity{6.0, 8.0, 10.0}, 1.0}};
        e.inputs = {ResourceFlowSpec{"fresh_water_l", VariableQuantity{20.0, 30.0, 40.0}, 0.5},
                    ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.5, 0.7, 1.0}, 0.5}};
        e.infrastructure.area_m2 = VariableQuantity{1.0};
        e.infrastructure.initial_cost = VariableQuantity{10.0};
        e.fertilization_per_m2["N_g"] = 10.0;
        e.fertilization_per_m2["P_g"] = 4.0;
        e.fertilization_per_m2["K_g"] = 8.0;
        (void)reg.register_entity(e);
    }

    return reg;
}

}  // anonymous namespace

// ── Scenario 1: broiler protein met from corn + bean (US1/P1) ─────────────────

TEST_CASE("Scenario 1: broiler protein and energy met from internal grain - no external purchase",
          "[solver][composition][US1]") {
    Registry reg = make_feed_registry();
    SolverConfig cfg;

    // Goals: broiler batch (1 instance) + corn goal creates grain surplus to feed it.
    // 1 corn plot produces 1.5 kg/year grain; goal consumes 0.09*12=1.08 kg → 0.42 kg surplus.
    // 0.42 kg corn @ {protein_g:93, energy_kcal:3580} covers {protein_g:4.65, energy_kcal:100}/yr.
    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{0.5}},
                                         ProductionGoal{"corn_grain_kg", VariableQuantity{0.09}}};

    auto result = homestead::solve(goals, reg, cfg);

    // Pass 1 must have routed grain protein/energy to broiler — no protein_g_external
    // or energy_kcal_external node needed when supply is sufficient.
    bool has_protein_gap = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::composition_gap && d.resource_slug.has_value() &&
               *d.resource_slug == "protein_g_external";
    });
    REQUIRE_FALSE(has_protein_gap);

    bool has_energy_gap = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::composition_gap && d.resource_slug.has_value() &&
               *d.resource_slug == "energy_kcal_external";
    });
    REQUIRE_FALSE(has_energy_gap);

    // composition_routed must be populated for at least one feed resource
    bool has_routed = std::ranges::any_of(result.balance_sheet, [](const ResourceBalance& b) {
        return !b.composition_routed.empty();
    });
    REQUIRE(has_routed);
}

// ── Scenario 2: corn N/P/K met from chicken manure (US2/P2) ──────────────────

TEST_CASE("Scenario 2: corn N-P-K met from chicken manure - no composition_gap",
          "[solver][composition][US2]") {
    Registry reg = make_fert_registry();
    SolverConfig cfg;

    std::vector<ProductionGoal> goals = {ProductionGoal{"broiler_meat_kg", VariableQuantity{5.0}},
                                         ProductionGoal{"corn_grain_kg", VariableQuantity{1.0}}};

    auto result = homestead::solve(goals, reg, cfg);

    // Chicken manure produced internally must supply N/P/K to corn.
    // No composition_gap diagnostic should be emitted for corn when manure is sufficient.
    bool has_npk_gap_for_corn = std::ranges::any_of(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::composition_gap && d.entity_slug.has_value() &&
               *d.entity_slug == "corn_plot_1m2";
    });
    REQUIRE_FALSE(has_npk_gap_for_corn);

    // composition_routed["N_g"] > 0 on chicken_manure_kg balance
    auto manure_it = std::ranges::find_if(result.balance_sheet, [](const ResourceBalance& b) {
        return b.resource_slug == "chicken_manure_kg";
    });
    REQUIRE(manure_it != result.balance_sheet.end());
    REQUIRE(manure_it->composition_routed.contains("N_g"));
    REQUIRE(manure_it->composition_routed.at("N_g") > 0.0);
}

// ── Scenario 3: partial fertilization via nutrient_water_l (US2/US3) ──────────

TEST_CASE("Scenario 3: partial fertilization - K_g_external gap with correct shortfall",
          "[solver][composition][US2][US3]") {
    Registry reg = make_fert_registry();
    SolverConfig cfg;

    std::vector<ProductionGoal> goals = {ProductionGoal{"tilapia_whole_kg", VariableQuantity{1.0}},
                                         ProductionGoal{"corn_grain_kg", VariableQuantity{1.0}}};

    auto result = homestead::solve(goals, reg, cfg);

    // nutrient_water_l has no K_g → full K demand must be purchased externally.
    auto k_diag = std::ranges::find_if(result.diagnostics, [](const Diagnostic& d) {
        return d.kind == DiagnosticKind::composition_gap && d.resource_slug.has_value() &&
               *d.resource_slug == "K_g_external";
    });
    REQUIRE(k_diag != result.diagnostics.end());
    REQUIRE(k_diag->shortfall_g.has_value());

    // Hand-calculated K shortfall:
    // corn_plot_1m2: fertilization_per_m2["K_g"]=6.0, area_m2=1.0, cycles_per_year=3.0
    // cpm = cycles_per_year / 12 = 3.0 / 12 = 0.25 (active for all 12 months)
    // corn_qty = ceil(goal_per_month / output_per_instance_per_month)
    //          = ceil(1.0 / (0.5 * 0.25)) = ceil(8.0) = 8
    // Since nutrient_water_l has zero K_g, shortfall = full annual K demand.
    const double k_per_m2 = 6.0;
    const double area_m2 = 1.0;
    const double corn_cycles_per_year = 3.0;
    const double cpm = corn_cycles_per_year / 12.0;        // = 0.25
    const double corn_qty = std::ceil(1.0 / (0.5 * cpm));  // = 8
    // Annual K demand = K_g/m2/cycle * area_m2 * qty * cycles_per_year
    const double expected_k_gap_g = k_per_m2 * area_m2 * corn_qty * corn_cycles_per_year;
    REQUIRE(*k_diag->shortfall_g == Approx(expected_k_gap_g).margin(0.01));

    // N and P were at least partially routed (nutrient_water_l has both)
    auto water_it = std::ranges::find_if(result.balance_sheet, [](const ResourceBalance& b) {
        return b.resource_slug == "nutrient_water_l";
    });
    if (water_it != result.balance_sheet.end()) {
        REQUIRE(water_it->composition_routed.contains("N_g"));
        REQUIRE(water_it->composition_routed.at("N_g") > 0.0);
    }
}

// ── Scenario 4: regression - no composition_requirements -> identical output ──

TEST_CASE("Scenario 4: plan with no composition fields produces unchanged solver output",
          "[solver][composition][US4]") {
    // Use the default registry but with a slug-only goal (tilapia) that has no
    // composition_requirements or fertilization_per_m2 on any involved entity.
    Registry reg = Registry::load_defaults();
    SolverConfig cfg;

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{20.0}}};

    auto result_with_feature = homestead::solve(goals, reg, cfg);

    // No composition_gap diagnostics must appear (no composition fields on tilapia path)
    bool has_comp_gap = std::ranges::any_of(
        result_with_feature.diagnostics,
        [](const Diagnostic& d) { return d.kind == DiagnosticKind::composition_gap; });
    REQUIRE_FALSE(has_comp_gap);

    // All ResourceBalance.composition_routed maps must be empty
    bool any_routed =
        std::ranges::any_of(result_with_feature.balance_sheet,
                            [](const ResourceBalance& b) { return !b.composition_routed.empty(); });
    REQUIRE_FALSE(any_routed);

    // loop_closure_score in [0, 1]
    REQUIRE(result_with_feature.loop_closure_score >= 0.0);
    REQUIRE(result_with_feature.loop_closure_score <= 1.0);
}

// ── Scenario 5: nutrient_water_l routes to lettuce without explicit slug link ──

TEST_CASE("Scenario 5: nutrient_water_l auto-routes to lettuce without explicit slug link",
          "[solver][composition][US3]") {
    Registry reg = make_fert_registry();
    SolverConfig cfg;

    std::vector<ProductionGoal> goals = {ProductionGoal{"tilapia_whole_kg", VariableQuantity{1.0}},
                                         ProductionGoal{"lettuce_head", VariableQuantity{10.0}}};

    auto result = homestead::solve(goals, reg, cfg);

    // nutrient_water_l should appear in the balance sheet with composition_routed["N_g"] > 0,
    // meaning it was used to fertilize lettuce even without an explicit slug link.
    auto water_it = std::ranges::find_if(result.balance_sheet, [](const ResourceBalance& b) {
        return b.resource_slug == "nutrient_water_l";
    });
    // The tilapia tank must be instantiated (it produces nutrient_water_l)
    bool has_tilapia = false;
    result.graph.for_each_node([&](NodeId /*id*/, const NodeVariant& v) {
        if (std::holds_alternative<EntityInstanceNode>(v)) {
            if (std::get<EntityInstanceNode>(v).entity_slug == "tilapia_tank_5000l") {
                has_tilapia = true;
            }
        }
    });
    REQUIRE(has_tilapia);

    // nutrient_water_l should be routed toward lettuce fertilization
    if (water_it != result.balance_sheet.end()) {
        REQUIRE(water_it->composition_routed.contains("N_g"));
        REQUIRE(water_it->composition_routed.at("N_g") > 0.0);
    }
}
