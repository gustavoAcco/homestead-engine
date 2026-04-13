// Tests for nutrient balance tracking (feature 003-npk-soil-balance).
// Six test cases covering unit conversion, deficit detection, nullopt semantics,
// isolation invariant (FR-008), JSON round-trip, and schema 1.0.x backward compat.

#include <homestead/core/registry.hpp>
#include <homestead/serialization/serialization.hpp>
#include <homestead/solver/solver.hpp>

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace homestead;
using Catch::Approx;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Build a minimal registry with one resource and one entity.
// Useful for unit-conversion tests that don't need the full default registry.
static Registry make_test_registry(const Resource& res, const Entity& ent) {
    Registry reg;
    (void)reg.register_resource(res);
    (void)reg.register_entity(ent);
    return reg;
}

// ── Test 1: Classic Sisteminha integration (T028) ─────────────────────────────
// Plan: bean_plot_1m2 + tilapia_tank_5000l (default registry, Scenario::expected).
//
// bean_plot_1m2 inputs only fresh_water_l and human_labor_hours — no compost —
// so the solver adds no fertilizer-producing entities beyond the tilapia tank.
// This gives a controlled, predetermined nutrient balance.
//
// Pre-calculated expected values (Embrapa reference, Scenario::expected):
//
//   Goals: bean_kg=1.0/month, tilapia_whole_kg=1.0/month
//
//   bean_plot_1m2: output 0.25 kg/cycle, 4 cycles/year, all months
//     monthly output per instance = 0.25 × (4/12) = 0.08333 kg/month
//     instances needed = ceil(1 / 0.08333) = 12
//     qty_bean = 12.0, area_m2 = 1.0, cycles_in_month = 4/12 = 0.3333
//     nutrient_demand: N=4.0, P=3.0, K=5.0 g/m²/cycle
//     demanded_n = 4.0 × 1.0 × 12 × (4/12) = 16.0 g/month  → N SURPLUS
//     demanded_p = 3.0 × 1.0 × 12 × (4/12) = 12.0 g/month  → P DEFICIT
//     demanded_k = 5.0 × 1.0 × 12 × (4/12) = 20.0 g/month  → K DEFICIT
//
//   tilapia_tank_5000l: output 50 kg/cycle, 3 cycles/year, all months
//     monthly output per instance = 50 × (3/12) = 12.5 kg/month
//     instances needed = ceil(1 / 12.5) = 1
//     qty_tilapia = 1.0
//     nutrient_water_l: 4800 L/cycle × (3/12) = 1200 L/month
//     N_ppm=30 → available_n = 1200 × 30/1000 = 36.0 g/month  → surplus
//     P_ppm=8  → available_p = 1200 × 8/1000  =  9.6 g/month  → deficit
//     no K_ppm → available_k = 0.0 g/month                    → deficit
//
//   Expected diagnostics: 12× Phosphorus deficit + 12× Potassium deficit, 0× Nitrogen

TEST_CASE("Sisteminha integration: bean + tilapia - P and K deficits, N surplus",
          "[solver][nutrient][integration]") {
    Registry reg = Registry::load_defaults();

    // Verify nutrient_demand is present on bean (requires T027 complete).
    auto bean_opt = reg.find_entity("bean_plot_1m2");
    REQUIRE(bean_opt.has_value());
    REQUIRE(bean_opt->nutrient_demand.has_value());
    REQUIRE(bean_opt->nutrient_demand->n_g_per_m2_per_cycle == Approx(4.0));
    REQUIRE(bean_opt->nutrient_demand->p_g_per_m2_per_cycle == Approx(3.0));
    REQUIRE(bean_opt->nutrient_demand->k_g_per_m2_per_cycle == Approx(5.0));

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"bean_kg", VariableQuantity{1.0}},
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{1.0}},
    };
    auto result = homestead::solve(goals, reg);

    // nutrient_balance must be populated (bean is a crop entity with nutrient_demand).
    REQUIRE(result.nutrient_balance.has_value());
    const auto& nb = *result.nutrient_balance;

    // Verify solver placed the expected entity quantities (pre-conditions for hard-coded values).
    double tilapia_qty = 0.0;
    double bean_qty = 0.0;
    result.graph.for_each_node([&](NodeId, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv))
            return;
        const auto& inst = std::get<EntityInstanceNode>(nv);
        if (inst.entity_slug == "tilapia_tank_5000l")
            tilapia_qty = inst.quantity.expected;
        if (inst.entity_slug == "bean_plot_1m2")
            bean_qty = inst.quantity.expected;
    });
    // ceil(1 bean_kg/month ÷ 0.08333 kg/month/instance) = 12
    REQUIRE(bean_qty == Approx(12.0).epsilon(1e-9));
    // ceil(1 tilapia_whole_kg/month ÷ 12.5 kg/month/instance) = 1
    REQUIRE(tilapia_qty == Approx(1.0).epsilon(1e-9));

    // Hard-coded expected values derived from registry data (Q5: fixed predetermined outcome).
    // Available supply — from tilapia's nutrient_water_l only (bean has no compost input).
    constexpr double expected_avail_n = 36.0;  // 1200 L × 30 ppm / 1000
    constexpr double expected_avail_p = 9.6;   // 1200 L × 8 ppm / 1000
    constexpr double expected_avail_k = 0.0;   // no K_ppm in nutrient_water_l

    // Demand — 12 bean instances × 1 m² × 4 cycles/year × (1/12 months/cycle)
    constexpr double expected_dem_n = 16.0;  // 4.0 × 1.0 × 12 × (4/12)
    constexpr double expected_dem_p = 12.0;  // 3.0 × 1.0 × 12 × (4/12)
    constexpr double expected_dem_k = 20.0;  // 5.0 × 1.0 × 12 × (4/12)

    // All 12 months are identical (both entities active all year, uniform lifecycles).
    for (int m = 0; m < 12; ++m) {
        INFO("month " << m);
        REQUIRE(nb.available_n[m] == Approx(expected_avail_n).epsilon(1e-6));
        REQUIRE(nb.available_p[m] == Approx(expected_avail_p).epsilon(1e-6));
        REQUIRE(nb.available_k[m] == Approx(expected_avail_k).margin(1e-9));
        REQUIRE(nb.demanded_n[m] == Approx(expected_dem_n).epsilon(1e-6));
        REQUIRE(nb.demanded_p[m] == Approx(expected_dem_p).epsilon(1e-6));
        REQUIRE(nb.demanded_k[m] == Approx(expected_dem_k).epsilon(1e-6));
    }

    // Count deficit diagnostics by element.
    int n_deficit_count = 0;
    int p_deficit_count = 0;
    int k_deficit_count = 0;
    for (const auto& d : result.diagnostics) {
        if (d.kind != DiagnosticKind::nutrient_deficit)
            continue;
        if (d.message.find("Nitrogen") != std::string::npos)
            ++n_deficit_count;
        if (d.message.find("Phosphorus") != std::string::npos)
            ++p_deficit_count;
        if (d.message.find("Potassium") != std::string::npos)
            ++k_deficit_count;
    }

    // N surplus → zero Nitrogen deficits.
    REQUIRE(n_deficit_count == 0);
    // P deficit (9.6 < 12.0) → one per active month = 12.
    REQUIRE(p_deficit_count == 12);
    // K deficit (0 < 20.0) → one per active month = 12.
    REQUIRE(k_deficit_count == 12);
}

// ── Test 4: Unit conversion correctness ──────────────────────────────────────
// (written first per Phase 3 parallel batch guidance)

TEST_CASE("Nutrient unit conversion: solid N_percent and liquid N_ppm",
          "[solver][nutrient][unit_conversion]") {
    // ── Solid resource: N_percent = 1.5, produce 100 kg net ──
    // Expected available_N = 100 * 1.5/100 * 1000 = 1500 g
    {
        Registry reg;

        Resource fert;
        fert.slug = "solid_fert_kg";
        fert.name = "Solid Fertilizer";
        fert.category = ResourceCategory::other;
        fert.physical = PhysicalProperties{1.0, 0.0, -1};
        fert.composition["N_percent"] = 1.5;
        (void)reg.register_resource(fert);

        Resource output;
        output.slug = "crop_yield_kg";
        output.name = "Crop Yield";
        output.category = ResourceCategory::food_product;
        output.physical = PhysicalProperties{1.0, 0.0, 3};
        (void)reg.register_resource(output);

        // Producer entity: outputs 100 kg/cycle of solid fert, 12 cycles/year, all months.
        // cycles_in_month = 12/12 = 1.0 → 100 kg/month net
        Entity producer;
        producer.slug = "fert_producer";
        producer.name = "Fert Producer";
        producer.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
        producer.outputs = {ResourceFlowSpec{"solid_fert_kg", VariableQuantity{100.0}, 1.0}};
        producer.infrastructure.area_m2 = VariableQuantity{1.0};
        (void)reg.register_entity(producer);

        // Crop entity with nutrient_demand: consumes solid fert.
        Resource dummy_crop_out;
        dummy_crop_out.slug = "dummy_crop_out";
        dummy_crop_out.name = "Dummy";
        dummy_crop_out.category = ResourceCategory::food_product;
        dummy_crop_out.physical = PhysicalProperties{1.0, 0.0, 3};
        (void)reg.register_resource(dummy_crop_out);

        Entity crop;
        crop.slug = "dummy_crop";
        crop.name = "Dummy Crop";
        crop.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
        crop.inputs = {ResourceFlowSpec{"solid_fert_kg", VariableQuantity{0.01}, 0.0}};
        crop.outputs = {ResourceFlowSpec{"dummy_crop_out", VariableQuantity{1.0}, 1.0}};
        crop.infrastructure.area_m2 = VariableQuantity{1.0};
        NutrientDemand nd;
        nd.n_g_per_m2_per_cycle = 1.0;  // small nonzero to trigger has_demand
        nd.p_g_per_m2_per_cycle = 0.0;
        nd.k_g_per_m2_per_cycle = 0.0;
        crop.nutrient_demand = nd;
        (void)reg.register_entity(crop);

        std::vector<ProductionGoal> goals = {
            ProductionGoal{"dummy_crop_out", VariableQuantity{1.0}}};
        auto result = homestead::solve(goals, reg);

        REQUIRE(result.nutrient_balance.has_value());
        // Supply: 100 kg/month * 1.5% * 1000 = 1500 g N for all months
        for (int m = 0; m < 12; ++m) {
            INFO("month " << m);
            REQUIRE(result.nutrient_balance->available_n[m] == Approx(1500.0).epsilon(1e-9));
            REQUIRE(result.nutrient_balance->available_p[m] == Approx(0.0).epsilon(1e-9));
            REQUIRE(result.nutrient_balance->available_k[m] == Approx(0.0).epsilon(1e-9));
        }
    }

    // ── Liquid resource: N_ppm = 30.0, produce 1000 L net ──
    // Expected available_N = 1000 * 30.0 / 1000 = 30 g
    {
        Registry reg;

        Resource liquid;
        liquid.slug = "liquid_fert_l";
        liquid.name = "Liquid Fertilizer";
        liquid.category = ResourceCategory::other;
        liquid.physical = PhysicalProperties{1.0, 1.0, -1};
        liquid.composition["N_ppm"] = 30.0;
        (void)reg.register_resource(liquid);

        Resource crop_out;
        crop_out.slug = "crop_out2";
        crop_out.name = "Crop Out 2";
        crop_out.category = ResourceCategory::food_product;
        crop_out.physical = PhysicalProperties{1.0, 0.0, 3};
        (void)reg.register_resource(crop_out);

        // Producer: outputs 1000 L/cycle, 12 cycles/year → 1000 L/month net
        Entity liq_producer;
        liq_producer.slug = "liquid_producer";
        liq_producer.name = "Liquid Producer";
        liq_producer.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
        liq_producer.outputs = {ResourceFlowSpec{"liquid_fert_l", VariableQuantity{1000.0}, 1.0}};
        liq_producer.infrastructure.area_m2 = VariableQuantity{1.0};
        (void)reg.register_entity(liq_producer);

        Entity crop2;
        crop2.slug = "dummy_crop2";
        crop2.name = "Dummy Crop 2";
        crop2.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
        crop2.inputs = {ResourceFlowSpec{"liquid_fert_l", VariableQuantity{0.01}, 0.0}};
        crop2.outputs = {ResourceFlowSpec{"crop_out2", VariableQuantity{1.0}, 1.0}};
        crop2.infrastructure.area_m2 = VariableQuantity{1.0};
        NutrientDemand nd2;
        nd2.n_g_per_m2_per_cycle = 1.0;
        crop2.nutrient_demand = nd2;
        (void)reg.register_entity(crop2);

        std::vector<ProductionGoal> goals = {ProductionGoal{"crop_out2", VariableQuantity{1.0}}};
        auto result = homestead::solve(goals, reg);

        REQUIRE(result.nutrient_balance.has_value());
        // Supply: 1000 L/month * 30.0 ppm / 1000 = 30 g N for all months
        for (int m = 0; m < 12; ++m) {
            INFO("month " << m);
            REQUIRE(result.nutrient_balance->available_n[m] == Approx(30.0).epsilon(1e-9));
            REQUIRE(result.nutrient_balance->available_p[m] == Approx(0.0).epsilon(1e-9));
        }
    }
}

// ── Test 3: No crop entities → nutrient_balance is nullopt ───────────────────

TEST_CASE("No crop entities: nutrient_balance is nullopt", "[solver][nutrient][nullopt]") {
    // Plan with only non-crop entities (broiler chickens + tilapia).
    // None have nutrient_demand set → no demand → nullopt.
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"broiler_meat_kg", VariableQuantity{2.0}},
        ProductionGoal{"tilapia_whole_kg", VariableQuantity{2.0}},
    };
    auto result = homestead::solve(goals, reg);

    // No area-based crop entity in this plan → nutrient_balance must be nullopt.
    REQUIRE_FALSE(result.nutrient_balance.has_value());
}

// ── Test 4b: Isolation invariant (FR-008) ─────────────────────────────────────
// Running the solver twice — once with nutrient_demand on a crop entity, once
// without — must produce identical balance_sheet quantities and graph node counts.

TEST_CASE("Isolation invariant: nutrient_demand does not alter balance sheet (FR-008)",
          "[solver][nutrient][isolation]") {
    Registry reg_with_demand = Registry::load_defaults();
    Registry reg_without_demand = Registry::load_defaults();

    // Remove nutrient_demand from corn in the second registry by building it fresh
    // from the same default data but leaving nutrient_demand as nullopt.
    // Since default registry doesn't have it yet (added in T027), both registries
    // load identically at this phase. We simulate by adding nutrient_demand to
    // a resource in reg_with_demand only.
    auto corn_opt = reg_with_demand.find_entity("corn_plot_1m2");
    REQUIRE(corn_opt.has_value());

    // Re-register with nutrient_demand set — this requires the entity to be
    // available for modification. We use a fresh registry built around corn only.
    Registry reg_a;
    Registry reg_b;

    // Build minimal shared resources
    Resource compost;
    compost.slug = "compost_kg";
    compost.name = "Compost";
    compost.category = ResourceCategory::other;
    compost.physical = PhysicalProperties{1.0, 0.0, -1};
    compost.composition["N_percent"] = 1.2;
    (void)reg_a.register_resource(compost);
    (void)reg_b.register_resource(compost);

    Resource corn_out;
    corn_out.slug = "corn_out_kg";
    corn_out.name = "Corn Output";
    corn_out.category = ResourceCategory::food_product;
    corn_out.physical = PhysicalProperties{1.0, 0.0, 14};
    (void)reg_a.register_resource(corn_out);
    (void)reg_b.register_resource(corn_out);

    Resource compost_producer_out;
    compost_producer_out.slug = "compost_producer_out";
    compost_producer_out.name = "Dummy manure";
    compost_producer_out.category = ResourceCategory::other;
    compost_producer_out.physical = PhysicalProperties{1.0, 0.0, -1};
    (void)reg_a.register_resource(compost_producer_out);
    (void)reg_b.register_resource(compost_producer_out);

    // Compost producer (no nutrient_demand)
    Entity compost_prod;
    compost_prod.slug = "compost_entity";
    compost_prod.name = "Compost Entity";
    compost_prod.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    compost_prod.outputs = {ResourceFlowSpec{"compost_kg", VariableQuantity{2.0}, 1.0},
                            ResourceFlowSpec{"compost_producer_out", VariableQuantity{0.1}, 1.0}};
    compost_prod.infrastructure.area_m2 = VariableQuantity{1.0};
    (void)reg_a.register_entity(compost_prod);
    (void)reg_b.register_entity(compost_prod);

    // Crop entity — WITH nutrient_demand in reg_a, WITHOUT in reg_b
    Entity corn_a;
    corn_a.slug = "corn_entity";
    corn_a.name = "Corn";
    corn_a.lifecycle = Lifecycle{0, 120, 3.0, ALL_MONTHS};
    corn_a.inputs = {ResourceFlowSpec{"compost_kg", VariableQuantity{0.5}, 0.0}};
    corn_a.outputs = {ResourceFlowSpec{"corn_out_kg", VariableQuantity{0.5}, 1.0}};
    corn_a.infrastructure.area_m2 = VariableQuantity{1.0};
    corn_a.nutrient_demand = NutrientDemand{8.0, 3.0, 6.0};
    (void)reg_a.register_entity(corn_a);

    Entity corn_b = corn_a;
    corn_b.nutrient_demand = std::nullopt;
    (void)reg_b.register_entity(corn_b);

    std::vector<ProductionGoal> goals = {ProductionGoal{"corn_out_kg", VariableQuantity{0.5}}};

    auto result_a = homestead::solve(goals, reg_a);
    auto result_b = homestead::solve(goals, reg_b);

    // nutrient_demand must not change quantities or graph topology.
    REQUIRE(result_a.graph.node_count() == result_b.graph.node_count());

    // balance_sheet quantities must be identical.
    REQUIRE(result_a.balance_sheet.size() == result_b.balance_sheet.size());
    for (const auto& bal_a : result_a.balance_sheet) {
        auto it = std::ranges::find_if(result_b.balance_sheet, [&](const ResourceBalance& b) {
            return b.resource_slug == bal_a.resource_slug;
        });
        REQUIRE(it != result_b.balance_sheet.end());
        for (int m = 0; m < 12; ++m) {
            INFO("resource=" << bal_a.resource_slug << " month=" << m);
            REQUIRE(bal_a.internal_production[m] ==
                    Approx(it->internal_production[m]).epsilon(1e-9));
        }
    }

    // Nutrient demand must differ (one has it, one doesn't).
    REQUIRE(result_a.nutrient_balance.has_value());
    REQUIRE_FALSE(result_b.nutrient_balance.has_value());
}

// ── Test 2: Nitrogen deficit diagnostic ──────────────────────────────────────
// Plan: bean_plot_1m2 only (no fertilizer producer in plan).
//
// bean_plot_1m2 inputs only fresh_water_l and human_labor_hours — no compost.
// Without a tilapia tank or other fertilizer entity, available_n = 0 every month.
// This produces a guaranteed nitrogen deficit across all 12 months.
//
// Pre-calculated expected values (default registry, Scenario::expected):
//   Goal: bean_kg = 1.0/month
//   bean_plot_1m2: output 0.25 kg/cycle × (4/12) = 0.08333 kg/month/instance
//     qty_bean = ceil(1.0 / 0.08333) = 12 instances
//     cycles_in_month = 4/12, area_m2 = 1.0
//     demanded_n = 4.0 × 1.0 × 12 × (4/12) = 16.0 g/month (all 12 months)
//   No N source → available_n = 0.0 g/month
//   Shortfall = 16.0 g/month → 12× Nitrogen deficit diagnostics

TEST_CASE("Nitrogen deficit: bean plot with no fertilizer source emits deficit diagnostics",
          "[solver][nutrient][deficit]") {
    Registry reg = Registry::load_defaults();

    // Verify nutrient_demand is present on bean (requires T027 complete).
    auto bean_opt = reg.find_entity("bean_plot_1m2");
    REQUIRE(bean_opt.has_value());
    REQUIRE(bean_opt->nutrient_demand.has_value());
    REQUIRE(bean_opt->nutrient_demand->n_g_per_m2_per_cycle == Approx(4.0));

    // Goal: bean only — no tilapia, no fertilizer entity added by solver.
    std::vector<ProductionGoal> goals = {
        ProductionGoal{"bean_kg", VariableQuantity{1.0}},
    };
    auto result = homestead::solve(goals, reg);

    REQUIRE(result.nutrient_balance.has_value());
    const auto& nb = *result.nutrient_balance;

    // Verify solver placed 12 bean instances (pre-condition for hard-coded demand values).
    double bean_qty = 0.0;
    result.graph.for_each_node([&](NodeId, const NodeVariant& nv) {
        if (!std::holds_alternative<EntityInstanceNode>(nv))
            return;
        const auto& inst = std::get<EntityInstanceNode>(nv);
        if (inst.entity_slug == "bean_plot_1m2")
            bean_qty = inst.quantity.expected;
    });
    REQUIRE(bean_qty == Approx(12.0).epsilon(1e-9));

    // No N source in plan → available_n = 0.0 every month.
    // demanded_n = 4.0 × 1.0 × 12 × (4/12) = 16.0 g/month.
    constexpr double expected_dem_n = 16.0;
    for (int m = 0; m < 12; ++m) {
        INFO("month " << m);
        REQUIRE(nb.available_n[m] == Approx(0.0).margin(1e-9));
        REQUIRE(nb.demanded_n[m] == Approx(expected_dem_n).epsilon(1e-6));
    }

    // Nitrogen deficit every month → exactly 12 Nitrogen deficit diagnostics.
    int n_deficit_count = 0;
    for (const auto& d : result.diagnostics) {
        if (d.kind == DiagnosticKind::nutrient_deficit &&
            d.message.find("Nitrogen") != std::string::npos) {
            ++n_deficit_count;
            // Verify message format: must contain "shortfall" and values > 0.
            REQUIRE(d.message.find("shortfall") != std::string::npos);
        }
    }
    REQUIRE(n_deficit_count == 12);
}

// ── Test 5: JSON round-trip ───────────────────────────────────────────────────

TEST_CASE("NutrientBalance JSON round-trip: all six arrays preserved exactly",
          "[solver][nutrient][roundtrip]") {
    // Construct a PlanResult with known non-trivial nutrient_balance.
    PlanResult plan;
    NutrientBalance nb;
    for (int m = 0; m < 12; ++m) {
        nb.available_n[m] = 100.0 + m * 1.1;
        nb.available_p[m] = 40.0 + m * 0.5;
        nb.available_k[m] = 80.0 + m * 0.7;
        nb.demanded_n[m] = 50.0 + m * 2.3;
        nb.demanded_p[m] = 20.0 + m * 1.1;
        nb.demanded_k[m] = 30.0 + m * 0.9;
    }
    plan.nutrient_balance = nb;

    // Serialize then deserialize.
    auto j = homestead::to_json(plan);
    auto restored_opt = homestead::plan_from_json(j);
    REQUIRE(restored_opt.has_value());
    const auto& restored = *restored_opt;

    REQUIRE(restored.nutrient_balance.has_value());
    const auto& rnb = *restored.nutrient_balance;

    for (int m = 0; m < 12; ++m) {
        INFO("month " << m);
        REQUIRE(rnb.available_n[m] == Approx(nb.available_n[m]).epsilon(1e-9));
        REQUIRE(rnb.available_p[m] == Approx(nb.available_p[m]).epsilon(1e-9));
        REQUIRE(rnb.available_k[m] == Approx(nb.available_k[m]).epsilon(1e-9));
        REQUIRE(rnb.demanded_n[m] == Approx(nb.demanded_n[m]).epsilon(1e-9));
        REQUIRE(rnb.demanded_p[m] == Approx(nb.demanded_p[m]).epsilon(1e-9));
        REQUIRE(rnb.demanded_k[m] == Approx(nb.demanded_k[m]).epsilon(1e-9));
    }
}

// ── Test 6: Schema 1.0.x backward compat ─────────────────────────────────────

TEST_CASE("Schema 1.0.x backward compat: absent nutrient_balance zero-initializes (Q2)",
          "[solver][nutrient][schema][compat]") {
    // Hard-coded JSON with version 1.0.0 and no nutrient_balance field.
    // Deserializer must produce NutrientBalance{} (zero-init), NOT nullopt.
    const std::string json_str = R"({
        "version": "1.0.0",
        "type": "plan_result",
        "data": {
            "labor_schedule": [0,0,0,0,0,0,0,0,0,0,0,0],
            "loop_closure_score": 0.0,
            "balance_sheet": [],
            "bom": {
                "total_area_m2": 0.0,
                "estimated_initial_cost": 0.0,
                "initial_labor_hours": 0.0,
                "materials": []
            },
            "diagnostics": []
        }
    })";

    auto j = nlohmann::json::parse(json_str);
    auto result = homestead::plan_from_json(j);
    REQUIRE(result.has_value());

    // Q2: absent nutrient_balance in 1.0.x file → zero-initialized struct, NOT nullopt.
    REQUIRE(result->nutrient_balance.has_value());
    const auto& nb = *result->nutrient_balance;
    for (int m = 0; m < 12; ++m) {
        INFO("month " << m);
        REQUIRE(nb.available_n[m] == 0.0);
        REQUIRE(nb.available_p[m] == 0.0);
        REQUIRE(nb.available_k[m] == 0.0);
        REQUIRE(nb.demanded_n[m] == 0.0);
        REQUIRE(nb.demanded_p[m] == 0.0);
        REQUIRE(nb.demanded_k[m] == 0.0);
    }
}
