#include <homestead/core/registry.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace homestead;

// ── Helper builders ────────────────────────────────────────────────────────────

static Resource make_resource(std::string slug) {
    Resource r;
    r.slug = std::move(slug);
    r.name = "Test Resource";
    r.category = ResourceCategory::other;
    r.physical = PhysicalProperties{1.0, 0.0, -1};
    return r;
}

static Entity make_entity(std::string slug, std::vector<std::string> output_slugs) {
    Entity e;
    e.slug = std::move(slug);
    e.name = "Test Entity";
    e.lifecycle = Lifecycle{0, 30, 12.0, ALL_MONTHS};
    e.infrastructure.area_m2 = VariableQuantity{1.0};
    for (auto& s : output_slugs) {
        e.outputs.push_back(ResourceFlowSpec{std::move(s), VariableQuantity{1.0}, 1.0});
    }
    return e;
}

// ── Basic registration ─────────────────────────────────────────────────────────

TEST_CASE("Registry starts empty", "[registry]") {
    Registry reg;
    REQUIRE(reg.resources().empty());
    REQUIRE(reg.entities().empty());
}

TEST_CASE("register_resource succeeds with valid slug", "[registry]") {
    Registry reg;
    auto result = reg.register_resource(make_resource("corn_grain_kg"));
    REQUIRE(result.has_value());
    REQUIRE(reg.resources().size() == 1);
}

TEST_CASE("register_resource rejects malformed slug", "[registry]") {
    Registry reg;
    auto result = reg.register_resource(make_resource("Corn Grain"));
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == RegistryErrorKind::malformed_slug);
    REQUIRE(reg.resources().empty());
}

TEST_CASE("register_resource override semantics", "[registry]") {
    Registry reg;
    auto r1 = make_resource("water");
    r1.name = "First Water";
    (void)reg.register_resource(r1);
    REQUIRE(reg.resources().size() == 1);

    auto r2 = make_resource("water");
    r2.name = "Second Water";
    (void)reg.register_resource(r2);
    REQUIRE(reg.resources().size() == 1);  // replaced, not appended
    REQUIRE(reg.find_resource("water")->name == "Second Water");
}

TEST_CASE("register_entity succeeds when all resource slugs exist", "[registry]") {
    Registry reg;
    (void)reg.register_resource(make_resource("meat_kg"));
    auto result = reg.register_entity(make_entity("chicken", {"meat_kg"}));
    REQUIRE(result.has_value());
    REQUIRE(reg.entities().size() == 1);
}

TEST_CASE("register_entity fails when output slug is unknown", "[registry]") {
    Registry reg;
    auto result = reg.register_entity(make_entity("chicken", {"unknown_resource"}));
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == RegistryErrorKind::unknown_resource_slug);
    REQUIRE(result.error().offending_slug == "unknown_resource");
}

TEST_CASE("register_entity fails when input slug is unknown", "[registry]") {
    Registry reg;
    (void)reg.register_resource(make_resource("output_kg"));

    Entity e = make_entity("my_entity", {"output_kg"});
    e.inputs.push_back(ResourceFlowSpec{"ghost_input", VariableQuantity{1.0}, 0.0});

    auto result = reg.register_entity(std::move(e));
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == RegistryErrorKind::unknown_resource_slug);
    REQUIRE(result.error().offending_slug == "ghost_input");
}

TEST_CASE("find_resource returns nullopt when not found", "[registry]") {
    Registry reg;
    REQUIRE_FALSE(reg.find_resource("nonexistent").has_value());
}

TEST_CASE("find_entity returns nullopt when not found", "[registry]") {
    Registry reg;
    REQUIRE_FALSE(reg.find_entity("nonexistent").has_value());
}

TEST_CASE("producers_of returns entity slugs", "[registry]") {
    Registry reg;
    (void)reg.register_resource(make_resource("meat_kg"));
    (void)reg.register_resource(make_resource("manure_kg"));
    (void)reg.register_entity(make_entity("chicken", {"meat_kg", "manure_kg"}));
    (void)reg.register_entity(make_entity("goat", {"meat_kg"}));

    auto producers = reg.producers_of("meat_kg");
    REQUIRE(producers.size() == 2);

    auto manure_producers = reg.producers_of("manure_kg");
    REQUIRE(manure_producers.size() == 1);
    REQUIRE(manure_producers[0] == "chicken");

    REQUIRE(reg.producers_of("nonexistent").empty());
}

// ── Default registry tests (T028) ──────────────────────────────────────────────

TEST_CASE("Registry::load_defaults contains all 20 expected entity slugs", "[registry][defaults]") {
    Registry reg = Registry::load_defaults();
    REQUIRE_FALSE(reg.resources().empty());
    REQUIRE_FALSE(reg.entities().empty());

    std::vector<std::string> expected_entities = {"broiler_chicken",
                                                  "laying_hen",
                                                  "quail",
                                                  "tilapia_tank_5000l",
                                                  "compost_bin",
                                                  "earthworm_bin",
                                                  "biodigester",
                                                  "lettuce_bed_1m2",
                                                  "tomato_bed_1m2",
                                                  "pepper_bed_1m2",
                                                  "corn_plot_1m2",
                                                  "bean_plot_1m2",
                                                  "cassava_plot_1m2",
                                                  "banana_plant",
                                                  "papaya_plant",
                                                  "acerola_plant",
                                                  "goat",
                                                  "goat_kids",
                                                  "water_tank",
                                                  "rainwater_collector"};

    for (const auto& slug : expected_entities) {
        INFO("Checking entity slug: " << slug);
        REQUIRE(reg.find_entity(slug).has_value());
    }
}

TEST_CASE("Default registry chicken_manure_kg resource has composition", "[registry][defaults]") {
    Registry reg = Registry::load_defaults();
    auto res = reg.find_resource("chicken_manure_kg");
    REQUIRE(res.has_value());
    REQUIRE_FALSE(res->composition.empty());
    REQUIRE(res->composition.count("N_percent") > 0);
}

// ── Custom entity registration (T029 — quickstart.md example) ─────────────────

TEST_CASE("Custom BSF bin entity registers and round-trips", "[registry][custom]") {
    Registry reg = Registry::load_defaults();

    // Register the new resource first
    auto res_result =
        reg.register_resource(Resource{"black_soldier_fly_larvae_kg",
                                       "Black Soldier Fly Larvae",
                                       ResourceCategory::feed,
                                       {},
                                       NutritionalProfile{3000.0, 420.0, 290.0, 0.0, {}},
                                       PhysicalProperties{1.0, 0.0, 3}});
    REQUIRE(res_result.has_value());

    // frass is a waste resource
    auto frass_result = reg.register_resource(Resource{"bsf_frass_kg",
                                                       "BSF Frass",
                                                       ResourceCategory::fertilizer,
                                                       {},
                                                       std::nullopt,
                                                       PhysicalProperties{1.0, 0.8, 90}});
    REQUIRE(frass_result.has_value());

    // Register the entity
    Entity bsf;
    bsf.slug = "bsf_bin";
    bsf.name = "Black Soldier Fly Bin";
    bsf.description = "Converts organic waste to high-protein larvae";
    bsf.inputs = {
        ResourceFlowSpec{"organic_waste_kg", VariableQuantity{10.0, 12.0, 15.0}, 0.0},
        ResourceFlowSpec{"human_labor_hours", VariableQuantity{0.5}, 0.5},
    };
    bsf.outputs = {
        ResourceFlowSpec{"black_soldier_fly_larvae_kg", VariableQuantity{1.0, 1.3, 1.6}, 1.0},
        ResourceFlowSpec{"bsf_frass_kg", VariableQuantity{3.0, 3.5, 4.0}, 1.0},
    };
    bsf.lifecycle = Lifecycle{7, 14, 26.0, ALL_MONTHS};
    bsf.operating_labor_per_cycle = VariableQuantity{0.5, 0.7, 1.0};
    bsf.infrastructure.area_m2 = VariableQuantity{0.5, 1.0, 2.0};
    bsf.infrastructure.initial_cost = VariableQuantity{50.0, 80.0, 120.0};

    auto ent_result = reg.register_entity(std::move(bsf));
    REQUIRE(ent_result.has_value());

    // Round-trip retrieval
    auto retrieved = reg.find_entity("bsf_bin");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->outputs.size() == 2);
    REQUIRE(retrieved->inputs.size() == 2);
    REQUIRE(retrieved->lifecycle.cycle_days == 14);
    REQUIRE(retrieved->lifecycle.cycles_per_year == 26.0);
}
