// Default tropical-agriculture registry.
// Source of truth: data/default_registry.json (kept in sync manually).
// Values sourced from Embrapa technical documents for the Sisteminha modules.
//
// This file is intentionally dependency-free (stdlib only) per constitution
// Principle II: homestead::core must compile with only the C++ standard library.

#include <homestead/core/registry.hpp>

namespace homestead::detail {

// Helper to force-register without error handling (data is known-good).
static void reg_resource(Registry& reg, Resource r) {
    (void)reg.register_resource(std::move(r));
}

static void reg_entity(Registry& reg, Entity e) {
    (void)reg.register_entity(std::move(e));
}

void build_default_registry(Registry& reg) {
    // ── Resources ─────────────────────────────────────────────────────────────
    // Poultry / livestock outputs
    reg_resource(reg, Resource{"broiler_meat_kg",
                               "Broiler Chicken Meat",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{2500, 210, 70, 0, {}},
                               PhysicalProperties{1.0, 0.0, 4}});
    reg_resource(reg, Resource{"egg",
                               "Chicken/Duck Egg",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{1550, 125, 110, 12, {}},
                               PhysicalProperties{0.06, 0.0, 21}});
    reg_resource(reg, Resource{"quail_egg",
                               "Quail Egg",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{1580, 130, 115, 10, {}},
                               PhysicalProperties{0.012, 0.0, 21}});
    reg_resource(reg, Resource{"chicken_manure_kg", "Chicken Manure", ResourceCategory::waste,
                               ChemicalComposition{
                                   {"N_percent", 1.5}, {"P_percent", 1.2}, {"K_percent", 0.8}},
                               std::nullopt, PhysicalProperties{1.0, 1.0, 90}});
    reg_resource(reg, Resource{"goat_milk_l",
                               "Goat Milk",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{690, 35, 45, 43, {}},
                               PhysicalProperties{1.03, 1.0, 3}});
    reg_resource(reg, Resource{"goat_meat_kg",
                               "Goat Meat",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{1090, 205, 32, 0, {}},
                               PhysicalProperties{1.0, 0.0, 3}});
    reg_resource(reg, Resource{"goat_manure_kg", "Goat Manure", ResourceCategory::waste,
                               ChemicalComposition{
                                   {"N_percent", 1.3}, {"P_percent", 0.8}, {"K_percent", 1.0}},
                               std::nullopt, PhysicalProperties{1.0, 0.8, 90}});
    reg_resource(reg, Resource{"goat_kids",
                               "Goat Kids (live)",
                               ResourceCategory::other,
                               {},
                               std::nullopt,
                               PhysicalProperties{3.0, 0.0, -1}});

    // Aquaculture
    reg_resource(reg, Resource{"tilapia_whole_kg",
                               "Whole Tilapia",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{960, 200, 15, 0, {}},
                               PhysicalProperties{1.0, 0.0, 1}});
    reg_resource(
        reg, Resource{"nutrient_water_l", "Nutrient-Rich Tank Water", ResourceCategory::fertilizer,
                      ChemicalComposition{{"N_ppm", 30.0}, {"P_ppm", 8.0}}, std::nullopt,
                      PhysicalProperties{1.0, 1.0, 7}});
    reg_resource(reg, Resource{"fish_feed_kg",
                               "Fish Feed",
                               ResourceCategory::feed,
                               {},
                               NutritionalProfile{3200, 320, 60, 200, {}},
                               PhysicalProperties{1.0, 2.0, 180}});
    reg_resource(reg, Resource{"tilapia_fingerlings",
                               "Tilapia Fingerlings",
                               ResourceCategory::other,
                               {},
                               std::nullopt,
                               PhysicalProperties{0.005, 0.0, -1}});

    // Soil / compost
    reg_resource(reg, Resource{"mature_compost_kg", "Mature Compost", ResourceCategory::fertilizer,
                               ChemicalComposition{{"N_percent", 1.2},
                                                   {"P_percent", 0.5},
                                                   {"K_percent", 0.8},
                                                   {"organic_matter_percent", 35.0}},
                               std::nullopt, PhysicalProperties{0.6, 0.6, -1}});
    reg_resource(reg, Resource{"vermicompost_kg", "Vermicompost", ResourceCategory::fertilizer,
                               ChemicalComposition{
                                   {"N_percent", 2.0}, {"P_percent", 1.2}, {"K_percent", 1.5}},
                               std::nullopt, PhysicalProperties{0.6, 0.5, -1}});
    reg_resource(reg, Resource{"earthworms_kg",
                               "Earthworms (live)",
                               ResourceCategory::other,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 0.8, -1}});
    reg_resource(reg, Resource{"organic_waste_kg",
                               "Organic Waste / Kitchen Scraps",
                               ResourceCategory::waste,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 1.0, 7}});
    reg_resource(reg, Resource{"green_material_kg",
                               "Green Plant Material (compost input)",
                               ResourceCategory::raw_material,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 3.0, 14}});

    // Energy
    reg_resource(reg, Resource{"biogas_m3", "Biogas", ResourceCategory::fuel,
                               ChemicalComposition{{"methane_percent", 60.0}}, std::nullopt,
                               PhysicalProperties{0.0007, 1.0, -1}});
    reg_resource(reg, Resource{"biofertilizer_l", "Liquid Biofertilizer (digestate)",
                               ResourceCategory::fertilizer,
                               ChemicalComposition{{"N_ppm", 800.0}, {"P_ppm", 200.0}},
                               std::nullopt, PhysicalProperties{1.0, 1.0, 30}});

    // Vegetables
    reg_resource(reg, Resource{"lettuce_head",
                               "Lettuce Head",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{150, 13, 2, 26, {}},
                               PhysicalProperties{0.25, 0.0, 7}});
    reg_resource(reg, Resource{"tomato_kg",
                               "Fresh Tomato",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{180, 9, 2, 39, {}},
                               PhysicalProperties{1.0, 0.0, 10}});
    reg_resource(reg, Resource{"pepper_kg",
                               "Fresh Pepper",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{310, 10, 3, 65, {}},
                               PhysicalProperties{1.0, 0.0, 14}});

    // Grains / legumes / roots / fruits
    reg_resource(reg, Resource{"corn_grain_kg",
                               "Corn Grain",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{3580, 93, 47, 730, {}},
                               PhysicalProperties{1.0, 1.2, 180}});
    reg_resource(reg, Resource{"corn_stover_kg",
                               "Corn Stover",
                               ResourceCategory::raw_material,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 4.0, 90}});
    reg_resource(reg, Resource{"bean_kg",
                               "Common Bean",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{3400, 220, 15, 600, {}},
                               PhysicalProperties{1.0, 1.1, 365}});
    reg_resource(reg, Resource{"bean_straw_kg",
                               "Bean Straw",
                               ResourceCategory::raw_material,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 3.0, 90}});
    reg_resource(reg, Resource{"cassava_kg",
                               "Cassava Root",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{1590, 14, 3, 380, {}},
                               PhysicalProperties{1.0, 0.0, 5}});
    reg_resource(reg, Resource{"cassava_leaves_kg",
                               "Cassava Leaves",
                               ResourceCategory::feed,
                               {},
                               NutritionalProfile{900, 200, 30, 80, {}},
                               PhysicalProperties{1.0, 2.0, 3}});
    reg_resource(reg, Resource{"banana_bunch_kg",
                               "Banana Bunch",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{890, 11, 3, 229, {}},
                               PhysicalProperties{1.0, 0.0, 10}});
    reg_resource(reg, Resource{"papaya_kg",
                               "Papaya",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{390, 6, 1, 98, {}},
                               PhysicalProperties{1.0, 0.0, 7}});
    reg_resource(reg, Resource{"acerola_kg",
                               "Acerola (Barbados Cherry)",
                               ResourceCategory::food_product,
                               {},
                               NutritionalProfile{320, 4, 3, 77, {}},
                               PhysicalProperties{1.0, 0.0, 3}});

    // Water
    reg_resource(reg, Resource{"stored_water_l",
                               "Stored Water",
                               ResourceCategory::water,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 1.0, -1}});
    reg_resource(reg, Resource{"fresh_water_l",
                               "Fresh Water (municipal/well)",
                               ResourceCategory::water,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 1.0, -1}});

    // Labor and money
    reg_resource(reg, Resource{"human_labor_hours",
                               "Human Labor",
                               ResourceCategory::labor_hours,
                               {},
                               std::nullopt,
                               PhysicalProperties{0.0, 0.0, -1}});
    reg_resource(reg, Resource{"brl",
                               "Brazilian Real (BRL)",
                               ResourceCategory::money,
                               {},
                               std::nullopt,
                               PhysicalProperties{0.0, 0.0, -1}});

    // Animal feed inputs
    reg_resource(reg, Resource{"poultry_feed_kg",
                               "Poultry Feed (commercial mix)",
                               ResourceCategory::feed,
                               {},
                               NutritionalProfile{2900, 180, 50, 450, {}},
                               PhysicalProperties{1.0, 1.2, 90}});
    reg_resource(reg, Resource{"goat_feed_kg",
                               "Goat Feed / Forage",
                               ResourceCategory::feed,
                               {},
                               NutritionalProfile{1800, 120, 25, 350, {}},
                               PhysicalProperties{1.0, 3.0, 14}});

    // Feathers
    reg_resource(reg, Resource{"feathers_kg",
                               "Poultry Feathers",
                               ResourceCategory::raw_material,
                               {},
                               std::nullopt,
                               PhysicalProperties{1.0, 10.0, 180}});

    // ── Entities ──────────────────────────────────────────────────────────────

    // broiler_chicken — 60-day cycle, 6 batches/year
    reg_entity(
        reg,
        Entity{"broiler_chicken",
               "Broiler Chicken Batch",
               "50-bird broiler pen",
               {
                   ResourceFlowSpec{"poultry_feed_kg", VariableQuantity{3.5, 4.0, 4.5}, 0.5},
                   ResourceFlowSpec{"fresh_water_l", VariableQuantity{8.0, 10.0, 12.0}, 0.5},
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.25, 0.3, 0.4}, 0.5},
               },
               {
                   ResourceFlowSpec{"broiler_meat_kg", VariableQuantity{1.5, 2.0, 2.5}, 1.0},
                   ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{1.0, 1.5, 2.0}, 0.5},
                   ResourceFlowSpec{"feathers_kg", VariableQuantity{0.05, 0.08, 0.12}, 1.0},
               },
               Lifecycle{7, 60, 6.0, ALL_MONTHS},
               VariableQuantity{0.3, 0.5, 0.7},
               VariableQuantity{2.0},
               InfrastructureSpec{VariableQuantity{0.1, 0.12, 0.15},
                                  {},
                                  VariableQuantity{4.0},
                                  VariableQuantity{50.0, 80.0, 120.0}}});

    // laying_hen — continuous, 300 eggs/yr per hen
    reg_entity(
        reg,
        Entity{"laying_hen",
               "Laying Hen",
               "Egg production unit (per hen, continuous)",
               {
                   ResourceFlowSpec{"poultry_feed_kg", VariableQuantity{0.1, 0.12, 0.14}, 0.5},
                   ResourceFlowSpec{"fresh_water_l", VariableQuantity{0.2, 0.25, 0.3}, 0.5},
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.02, 0.03, 0.04}, 0.5},
               },
               {
                   ResourceFlowSpec{"egg", VariableQuantity{20.0, 25.0, 28.0}, 0.5},
                   ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{1.5, 2.0, 2.5}, 0.5},
               },
               Lifecycle{0, 30, 12.0, ALL_MONTHS},
               VariableQuantity{0.1, 0.15, 0.2},
               VariableQuantity{4.0},
               InfrastructureSpec{VariableQuantity{0.25, 0.3, 0.4},
                                  {},
                                  VariableQuantity{2.0},
                                  VariableQuantity{20.0, 30.0, 50.0}}});

    // quail — continuous, 280 eggs/yr per bird
    reg_entity(
        reg,
        Entity{"quail",
               "Japanese Quail",
               "Egg production (per bird, continuous)",
               {
                   ResourceFlowSpec{"poultry_feed_kg", VariableQuantity{0.025, 0.028, 0.032}, 0.5},
                   ResourceFlowSpec{"fresh_water_l", VariableQuantity{0.05, 0.06, 0.07}, 0.5},
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.005, 0.007, 0.01}, 0.5},
               },
               {
                   ResourceFlowSpec{"quail_egg", VariableQuantity{18.0, 22.0, 25.0}, 0.5},
                   ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{0.3, 0.4, 0.5}, 0.5},
               },
               Lifecycle{0, 30, 12.0, ALL_MONTHS},
               VariableQuantity{0.02, 0.025, 0.03},
               VariableQuantity{40.0},
               InfrastructureSpec{VariableQuantity{0.025, 0.03, 0.04},
                                  {},
                                  VariableQuantity{1.0},
                                  VariableQuantity{5.0, 8.0, 12.0}}});

    // tilapia_tank_5000l — 120-day cycle, 3 batches/year
    reg_entity(
        reg,
        Entity{
            "tilapia_tank_5000l",
            "Tilapia Tank (5000L)",
            "5000-litre tilapia grow-out tank",
            {
                ResourceFlowSpec{"fish_feed_kg", VariableQuantity{30.0, 35.0, 40.0}, 0.5},
                ResourceFlowSpec{"fresh_water_l", VariableQuantity{200.0, 250.0, 300.0}, 0.3},
                ResourceFlowSpec{"tilapia_fingerlings", VariableQuantity{150.0, 200.0, 250.0}, 0.0},
                ResourceFlowSpec{"human_labor_hours", VariableQuantity{2.0, 3.0, 4.0}, 0.5},
            },
            {
                ResourceFlowSpec{"tilapia_whole_kg", VariableQuantity{40.0, 50.0, 60.0}, 1.0},
                ResourceFlowSpec{"nutrient_water_l", VariableQuantity{4500.0, 4800.0, 5000.0}, 1.0},
            },
            Lifecycle{7, 120, 3.0, ALL_MONTHS},
            VariableQuantity{2.0, 3.0, 4.0},
            VariableQuantity{40.0},
            InfrastructureSpec{VariableQuantity{6.0, 7.0, 8.0},
                               {},
                               VariableQuantity{8.0},
                               VariableQuantity{500.0, 800.0, 1200.0}}});

    // compost_bin — 90-day cycle
    reg_entity(
        reg,
        Entity{"compost_bin",
               "Compost Bin",
               "Open compost pile, 1 m³",
               {
                   ResourceFlowSpec{"organic_waste_kg", VariableQuantity{20.0, 25.0, 30.0}, 0.1},
                   ResourceFlowSpec{"green_material_kg", VariableQuantity{10.0, 12.0, 15.0}, 0.1},
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{1.0, 1.5, 2.0}, 0.5},
               },
               {
                   ResourceFlowSpec{"mature_compost_kg", VariableQuantity{15.0, 20.0, 25.0}, 1.0},
               },
               Lifecycle{0, 90, 4.0, ALL_MONTHS},
               VariableQuantity{0.3, 0.5, 0.7},
               VariableQuantity{1.0},
               InfrastructureSpec{VariableQuantity{1.0, 1.2, 1.5},
                                  {},
                                  VariableQuantity{2.0},
                                  VariableQuantity{10.0, 20.0, 40.0}}});

    // earthworm_bin — continuous
    reg_entity(
        reg, Entity{"earthworm_bin",
                    "Earthworm Bin (Vermicomposting)",
                    "1m² worm bed",
                    {
                        ResourceFlowSpec{"organic_waste_kg", VariableQuantity{5.0, 7.0, 9.0}, 0.2},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.2, 0.3, 0.4}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"vermicompost_kg", VariableQuantity{2.0, 3.0, 4.0}, 1.0},
                        ResourceFlowSpec{"earthworms_kg", VariableQuantity{0.1, 0.2, 0.3}, 1.0},
                    },
                    Lifecycle{0, 30, 12.0, ALL_MONTHS},
                    VariableQuantity{0.1, 0.15, 0.2},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{1.0, 1.0, 1.2},
                                       {},
                                       VariableQuantity{2.0},
                                       VariableQuantity{20.0, 30.0, 50.0}}});

    // biodigester — continuous
    reg_entity(
        reg,
        Entity{"biodigester",
               "Biodigester",
               "Fixed-dome biodigester, 1000L",
               {
                   ResourceFlowSpec{"organic_waste_kg", VariableQuantity{8.0, 10.0, 12.0}, 0.1},
                   ResourceFlowSpec{"chicken_manure_kg", VariableQuantity{2.0, 3.0, 4.0}, 0.1},
                   ResourceFlowSpec{"fresh_water_l", VariableQuantity{20.0, 25.0, 30.0}, 0.1},
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.1, 0.15, 0.2}, 0.5},
               },
               {
                   ResourceFlowSpec{"biogas_m3", VariableQuantity{0.3, 0.5, 0.7}, 1.0},
                   ResourceFlowSpec{"biofertilizer_l", VariableQuantity{20.0, 25.0, 30.0}, 1.0},
               },
               Lifecycle{30, 30, 12.0, ALL_MONTHS},
               VariableQuantity{0.1, 0.15, 0.2},
               VariableQuantity{1.0},
               InfrastructureSpec{VariableQuantity{2.0, 2.5, 3.0},
                                  {},
                                  VariableQuantity{20.0},
                                  VariableQuantity{800.0, 1200.0, 2000.0}}});

    // lettuce_bed_1m2 — 45-day cycle, active all months in tropics
    reg_entity(
        reg, Entity{"lettuce_bed_1m2",
                    "Lettuce Bed (1 m²)",
                    "Direct-seeded or transplanted lettuce",
                    {
                        ResourceFlowSpec{"mature_compost_kg", VariableQuantity{0.5, 0.8, 1.0}, 0.0},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{20.0, 30.0, 40.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.5, 0.7, 1.0}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"lettuce_head", VariableQuantity{6.0, 8.0, 10.0}, 1.0},
                    },
                    Lifecycle{0, 45, 8.0, ALL_MONTHS},
                    VariableQuantity{0.3, 0.5, 0.8},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{1.0, 1.0, 1.0},
                                       {},
                                       VariableQuantity{0.5},
                                       VariableQuantity{5.0, 10.0, 20.0}}});

    // tomato_bed_1m2 — 90-day cycle
    reg_entity(
        reg, Entity{"tomato_bed_1m2",
                    "Tomato Bed (1 m²)",
                    "Staked tomato production",
                    {
                        ResourceFlowSpec{"mature_compost_kg", VariableQuantity{1.0, 1.5, 2.0}, 0.0},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{30.0, 40.0, 50.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{1.0, 1.5, 2.0}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"tomato_kg", VariableQuantity{3.0, 5.0, 8.0}, 1.0},
                    },
                    Lifecycle{0, 90, 4.0, ALL_MONTHS},
                    VariableQuantity{0.5, 0.8, 1.2},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{1.0, 1.0, 1.0},
                                       {},
                                       VariableQuantity{1.0},
                                       VariableQuantity{10.0, 15.0, 25.0}}});

    // pepper_bed_1m2 — 75-day cycle
    reg_entity(
        reg, Entity{"pepper_bed_1m2",
                    "Pepper Bed (1 m²)",
                    "Bell pepper or hot pepper production",
                    {
                        ResourceFlowSpec{"mature_compost_kg", VariableQuantity{0.8, 1.0, 1.5}, 0.0},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{25.0, 35.0, 45.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.8, 1.0, 1.5}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"pepper_kg", VariableQuantity{1.5, 2.5, 4.0}, 1.0},
                    },
                    Lifecycle{0, 75, 4.8, ALL_MONTHS},
                    VariableQuantity{0.4, 0.6, 0.9},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{1.0, 1.0, 1.0},
                                       {},
                                       VariableQuantity{1.0},
                                       VariableQuantity{8.0, 12.0, 20.0}}});

    // corn_plot_1m2 — 120-day cycle
    reg_entity(
        reg, Entity{"corn_plot_1m2",
                    "Corn Plot (1 m²)",
                    "Open-pollinated corn, 1 m² plot",
                    {
                        ResourceFlowSpec{"mature_compost_kg", VariableQuantity{0.3, 0.5, 0.7}, 0.0},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{30.0, 40.0, 50.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.3, 0.5, 0.7}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"corn_grain_kg", VariableQuantity{0.3, 0.5, 0.7}, 1.0},
                        ResourceFlowSpec{"corn_stover_kg", VariableQuantity{0.5, 0.8, 1.0}, 1.0},
                    },
                    Lifecycle{0, 120, 3.0, ALL_MONTHS},
                    VariableQuantity{0.2, 0.3, 0.5},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{1.0, 1.0, 1.0},
                                       {},
                                       VariableQuantity{0.5},
                                       VariableQuantity{2.0, 5.0, 10.0}}});

    // bean_plot_1m2 — 90-day cycle
    reg_entity(
        reg, Entity{"bean_plot_1m2",
                    "Common Bean Plot (1 m²)",
                    "Climbing or bush bean",
                    {
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{20.0, 30.0, 40.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.2, 0.3, 0.5}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"bean_kg", VariableQuantity{0.15, 0.25, 0.35}, 1.0},
                        ResourceFlowSpec{"bean_straw_kg", VariableQuantity{0.3, 0.5, 0.7}, 1.0},
                    },
                    Lifecycle{0, 90, 4.0, ALL_MONTHS},
                    VariableQuantity{0.1, 0.2, 0.3},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{1.0, 1.0, 1.0},
                                       {},
                                       VariableQuantity{0.3},
                                       VariableQuantity{2.0, 3.0, 5.0}}});

    // cassava_plot_1m2 — 365-day cycle (annual)
    reg_entity(
        reg, Entity{"cassava_plot_1m2",
                    "Cassava Plot (1 m²)",
                    "Cassava (manioc) annual production",
                    {
                        ResourceFlowSpec{"mature_compost_kg", VariableQuantity{0.2, 0.3, 0.5}, 0.0},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{15.0, 20.0, 30.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.3, 0.5, 0.8}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"cassava_kg", VariableQuantity{1.5, 2.5, 4.0}, 1.0},
                        ResourceFlowSpec{"cassava_leaves_kg", VariableQuantity{0.3, 0.5, 0.8}, 0.5},
                    },
                    Lifecycle{0, 365, 1.0, ALL_MONTHS},
                    VariableQuantity{0.2, 0.4, 0.6},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{1.0, 1.0, 1.0},
                                       {},
                                       VariableQuantity{0.5},
                                       VariableQuantity{2.0, 3.0, 5.0}}});

    // banana_plant — 365-day first cycle, then ratoon
    reg_entity(
        reg,
        Entity{"banana_plant",
               "Banana Plant",
               "Banana (cavendish-type), per plant",
               {
                   ResourceFlowSpec{"mature_compost_kg", VariableQuantity{1.0, 2.0, 3.0}, 0.0},
                   ResourceFlowSpec{"fresh_water_l", VariableQuantity{80.0, 100.0, 130.0}, 0.5},
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.3, 0.5, 0.7}, 0.5},
               },
               {
                   ResourceFlowSpec{"banana_bunch_kg", VariableQuantity{10.0, 15.0, 20.0}, 1.0},
               },
               Lifecycle{0, 365, 1.0, ALL_MONTHS},
               VariableQuantity{0.2, 0.3, 0.5},
               VariableQuantity{1.0},
               InfrastructureSpec{VariableQuantity{4.0, 4.0, 6.0},
                                  {},
                                  VariableQuantity{1.0},
                                  VariableQuantity{5.0, 8.0, 15.0}}});

    // papaya_plant — continuous fruiting
    reg_entity(
        reg, Entity{"papaya_plant",
                    "Papaya Plant",
                    "Continuous fruiting papaya, per plant",
                    {
                        ResourceFlowSpec{"mature_compost_kg", VariableQuantity{0.5, 0.8, 1.2}, 0.0},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{50.0, 70.0, 90.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.1, 0.2, 0.3}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"papaya_kg", VariableQuantity{3.0, 5.0, 8.0}, 1.0},
                    },
                    Lifecycle{0, 30, 12.0, ALL_MONTHS},
                    VariableQuantity{0.1, 0.15, 0.2},
                    VariableQuantity{0.25},
                    InfrastructureSpec{VariableQuantity{4.0, 4.0, 6.0},
                                       {},
                                       VariableQuantity{0.5},
                                       VariableQuantity{5.0, 8.0, 15.0}}});

    // acerola_plant — continuous
    reg_entity(
        reg, Entity{"acerola_plant",
                    "Acerola Plant (Barbados Cherry)",
                    "Per plant, continuous",
                    {
                        ResourceFlowSpec{"mature_compost_kg", VariableQuantity{0.3, 0.5, 0.8}, 0.0},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{30.0, 40.0, 60.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.1, 0.2, 0.3}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"acerola_kg", VariableQuantity{2.0, 3.5, 5.0}, 1.0},
                    },
                    Lifecycle{0, 30, 12.0, ALL_MONTHS},
                    VariableQuantity{0.1, 0.15, 0.2},
                    VariableQuantity{0.17},
                    InfrastructureSpec{VariableQuantity{6.0, 6.0, 9.0},
                                       {},
                                       VariableQuantity{0.5},
                                       VariableQuantity{8.0, 12.0, 20.0}}});

    // goat — continuous (milk + kids)
    reg_entity(
        reg, Entity{"goat",
                    "Dairy Goat",
                    "Doe (female), continuous milk production",
                    {
                        ResourceFlowSpec{"goat_feed_kg", VariableQuantity{1.2, 1.5, 1.8}, 0.5},
                        ResourceFlowSpec{"fresh_water_l", VariableQuantity{3.0, 4.0, 5.0}, 0.5},
                        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.3, 0.4, 0.6}, 0.5},
                    },
                    {
                        ResourceFlowSpec{"goat_milk_l", VariableQuantity{1.5, 2.0, 2.5}, 0.5},
                        ResourceFlowSpec{"goat_manure_kg", VariableQuantity{1.0, 1.5, 2.0}, 0.5},
                        ResourceFlowSpec{"goat_kids", VariableQuantity{0.17, 0.17, 0.17}, 1.0},
                    },
                    Lifecycle{0, 30, 12.0, ALL_MONTHS},
                    VariableQuantity{0.2, 0.3, 0.4},
                    VariableQuantity{1.0},
                    InfrastructureSpec{VariableQuantity{2.0, 2.5, 3.0},
                                       {},
                                       VariableQuantity{4.0},
                                       VariableQuantity{200.0, 300.0, 500.0}}});

    // goat_kids — 150-day grow-out
    reg_entity(
        reg,
        Entity{"goat_kids",
               "Goat Kid Grow-out",
               "Kids raised for meat (per kid)",
               {
                   ResourceFlowSpec{"goat_feed_kg", VariableQuantity{0.4, 0.5, 0.6}, 0.5},
                   ResourceFlowSpec{"fresh_water_l", VariableQuantity{1.0, 1.5, 2.0}, 0.5},
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.05, 0.08, 0.12}, 0.5},
               },
               {
                   ResourceFlowSpec{"goat_meat_kg", VariableQuantity{8.0, 12.0, 16.0}, 1.0},
                   ResourceFlowSpec{"goat_manure_kg", VariableQuantity{0.5, 0.8, 1.0}, 0.5},
               },
               Lifecycle{0, 150, 2.4, ALL_MONTHS},
               VariableQuantity{0.05, 0.08, 0.12},
               VariableQuantity{1.0},
               InfrastructureSpec{VariableQuantity{1.0, 1.5, 2.0},
                                  {},
                                  VariableQuantity{1.0},
                                  VariableQuantity{50.0, 80.0, 120.0}}});

    // water_tank — infrastructure, no cycle
    reg_entity(
        reg,
        Entity{"water_tank",
               "Water Storage Tank",
               "Concrete or polyethylene tank, 1000L",
               {},
               {
                   ResourceFlowSpec{"stored_water_l", VariableQuantity{800.0, 1000.0, 1000.0}, 0.5},
               },
               Lifecycle{7, 30, 12.0, ALL_MONTHS},
               VariableQuantity{0.02, 0.03, 0.05},
               VariableQuantity{0.0},
               InfrastructureSpec{VariableQuantity{1.0, 1.5, 2.0},
                                  {},
                                  VariableQuantity{4.0},
                                  VariableQuantity{150.0, 250.0, 400.0}}});

    // rainwater_collector — infrastructure
    reg_entity(
        reg,
        Entity{"rainwater_collector",
               "Rainwater Collector",
               "Roof catchment + cistern, 500L",
               {
                   ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.02, 0.03, 0.05}, 0.5},
               },
               {
                   ResourceFlowSpec{"stored_water_l", VariableQuantity{200.0, 400.0, 600.0}, 0.5},
               },
               Lifecycle{14, 30, 12.0, ALL_MONTHS},
               VariableQuantity{0.02, 0.03, 0.05},
               VariableQuantity{0.0},
               InfrastructureSpec{VariableQuantity{0.0, 0.0, 0.0},
                                  {},
                                  VariableQuantity{8.0},
                                  VariableQuantity{200.0, 350.0, 600.0}}});
}

}  // namespace homestead::detail
