#include <homestead/serialization/schema_version.hpp>
#include <homestead/serialization/serialization.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace homestead;
using nlohmann::json;

// ── SchemaVersion parsing ─────────────────────────────────────────────────────

TEST_CASE("SchemaVersion::parse valid strings", "[serialization][schema]") {
    auto v = SchemaVersion::parse("1.2.3");
    REQUIRE(v.has_value());
    REQUIRE(v->major == 1);
    REQUIRE(v->minor == 2);
    REQUIRE(v->patch == 3);

    auto v2 = SchemaVersion::parse("1.0.0");
    REQUIRE(v2.has_value());
    REQUIRE(v2->major == 1);
}

TEST_CASE("SchemaVersion::parse rejects malformed strings", "[serialization][schema]") {
    REQUIRE_FALSE(SchemaVersion::parse("1.0").has_value());
    REQUIRE_FALSE(SchemaVersion::parse("abc.def.ghi").has_value());
    REQUIRE_FALSE(SchemaVersion::parse("").has_value());
}

TEST_CASE("SchemaVersion::to_string round-trips", "[serialization][schema]") {
    SchemaVersion v{2, 3, 4};
    REQUIRE(v.to_string() == "2.3.4");
}

TEST_CASE("SchemaVersion::compatible_with checks major only", "[serialization][schema]") {
    SchemaVersion a{1, 0, 0}, b{1, 5, 2}, c{2, 0, 0};
    REQUIRE(a.compatible_with(b));
    REQUIRE(b.compatible_with(a));
    REQUIRE_FALSE(a.compatible_with(c));
}

// ── Document-level version checks ────────────────────────────────────────────

TEST_CASE("registry_from_json: missing version field returns error", "[serialization][schema]") {
    json j = {{"type", "registry"},
              {"data", {{"resources", json::array()}, {"entities", json::array()}}}};
    auto result = registry_from_json(j);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().find("version") != std::string::npos);
}

TEST_CASE("registry_from_json: incompatible major version returns error",
          "[serialization][schema]") {
    json j = {{"version", "9.0.0"},
              {"type", "registry"},
              {"data", {{"resources", json::array()}, {"entities", json::array()}}}};
    auto result = registry_from_json(j);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().find("incompatible") != std::string::npos);
}

TEST_CASE("registry_from_json: valid current version deserializes cleanly",
          "[serialization][schema]") {
    json j = {{"version", "1.0.0"},
              {"type", "registry"},
              {"data", {{"resources", json::array()}, {"entities", json::array()}}}};
    auto result = registry_from_json(j);
    REQUIRE(result.has_value());
    REQUIRE(result->resources().empty());
    REQUIRE(result->entities().empty());
}

TEST_CASE("registry_from_json: malformed VariableQuantity returns descriptive error",
          "[serialization][schema]") {
    json res_j = json::array();
    json r;
    r["slug"] = "test_res";
    r["name"] = "Test";
    r["category"] = 0;
    r["physical"]["weight_kg_per_unit"] = 1.0;
    r["physical"]["volume_liters_per_unit"] = 0.0;
    r["physical"]["shelf_life_days"] = -1;
    res_j.push_back(r);

    json ent_j = json::array();
    json e;
    e["slug"] = "bad_entity";
    e["name"] = "Bad";
    e["outputs"] = json::array(
        {{{"resource_slug", "test_res"}, {"quantity_per_cycle", "not_an_array"}, {"timing", 0.0}}});
    ent_j.push_back(e);

    json j = {{"version", "1.0.0"},
              {"type", "registry"},
              {"data", {{"resources", res_j}, {"entities", ent_j}}}};

    auto result = registry_from_json(j);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(!result.error().empty());
}

TEST_CASE("graph_from_json: missing version returns error", "[serialization][schema]") {
    json j = {{"data", {{"nodes", json::array()}, {"edges", json::array()}}}};
    auto result = graph_from_json(j);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("plan_from_json: missing version returns error", "[serialization][schema]") {
    json j = {{"data", {}}};
    auto result = plan_from_json(j);
    REQUIRE_FALSE(result.has_value());
}
