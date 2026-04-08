// Performance benchmark — validates SC-001 and SC-002.
// Run via: ctest --preset debug -R bench_

#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace homestead;
using namespace std::chrono;

// ── SC-001: small plan < 100 ms ───────────────────────────────────────────────

TEST_CASE("SC-001: small plan (3 goals) completes in < 100 ms", "[bench][timing]") {
    Registry reg = Registry::load_defaults();

    std::vector<ProductionGoal> goals = {
        ProductionGoal{"broiler_meat_kg", VariableQuantity{5.0}},
        ProductionGoal{"mature_compost_kg", VariableQuantity{10.0}},
        ProductionGoal{"corn_grain_kg", VariableQuantity{3.0}},
    };

    auto start = high_resolution_clock::now();
    auto result = homestead::solve(goals, reg);
    auto end = high_resolution_clock::now();

    auto elapsed_ms = duration_cast<milliseconds>(end - start).count();
    INFO("Elapsed: " << elapsed_ms << " ms");
    REQUIRE(elapsed_ms < 100);
    REQUIRE(result.graph.node_count() > 0);
}

// ── SC-002: large synthetic plan < 1 s ────────────────────────────────────────

TEST_CASE("SC-002: large synthetic plan (all 20 entities) completes in < 1000 ms",
          "[bench][timing]") {
    Registry reg = Registry::load_defaults();

    // Request every food-product resource in the default registry.
    std::vector<ProductionGoal> goals;
    for (const auto& entity : reg.entities()) {
        for (const auto& out : entity.outputs) {
            auto res = reg.find_resource(out.resource_slug);
            if (res && res->category == ResourceCategory::food_product) {
                goals.push_back(ProductionGoal{out.resource_slug, VariableQuantity{1.0}});
            }
        }
    }
    // Deduplicate.
    std::sort(goals.begin(), goals.end(),
              [](const auto& a, const auto& b) { return a.resource_slug < b.resource_slug; });
    goals.erase(std::unique(goals.begin(), goals.end(),
                            [](const auto& a, const auto& b) {
                                return a.resource_slug == b.resource_slug;
                            }),
                goals.end());

    auto start = high_resolution_clock::now();
    auto result = homestead::solve(goals, reg);
    auto end = high_resolution_clock::now();

    auto elapsed_ms = duration_cast<milliseconds>(end - start).count();
    INFO("Elapsed: " << elapsed_ms << " ms  |  Goals: " << goals.size()
                     << "  |  Nodes: " << result.graph.node_count());
    REQUIRE(elapsed_ms < 1000);
}
