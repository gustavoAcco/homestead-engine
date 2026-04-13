# Quickstart: Quantity Scaling Solver

**Feature**: 002-quantity-scaling-solver  
**Date**: 2026-04-08

## What changed

Before this feature, `solve()` assigned `quantity = 1.0` to every entity in the plan regardless of the goal amount. After this feature, quantities reflect the actual number of units needed to meet each production goal.

## Basic usage (unchanged API)

```cpp
#include <homestead/core/registry.hpp>
#include <homestead/solver/solver.hpp>

// Load the built-in registry with ~20 tropical agriculture entities.
homestead::Registry reg = homestead::Registry::load_defaults();

// Define production goals.
std::vector<homestead::ProductionGoal> goals = {
    {"broiler_meat_kg",  homestead::VariableQuantity{50.0}},   // 50 kg/month
    {"egg",              homestead::VariableQuantity{200.0}},  // 200 eggs/month
    {"tilapia_whole_kg", homestead::VariableQuantity{30.0}},   // 30 kg/month
};

// Run the solver with default configuration.
homestead::SolverConfig cfg;  // expected scenario, no area/budget limits
homestead::PlanResult plan = homestead::solve(goals, reg, cfg);

// Inspect the results.
for (const auto& bal : plan.balance_sheet) {
    // internal_production and consumption are now scaled to actual entity counts.
}
std::cout << "Loop closure score: " << plan.loop_closure_score << '\n';
```

## Reading entity quantities

```cpp
plan.graph.for_each_node([](homestead::NodeId id, const homestead::NodeVariant& node) {
    if (std::holds_alternative<homestead::EntityInstanceNode>(node)) {
        const auto& inst = std::get<homestead::EntityInstanceNode>(node);
        // quantity.expected is now a meaningful integer-valued double.
        std::cout << inst.entity_slug << ": "
                  << inst.quantity.expected << " units\n";
    }
});
```

## Hand-calculated validation scenario

For a goal of **50 kg broiler_meat_kg/month** with the default registry (expected scenario):

| Entity | Expected quantity | Reasoning |
|---|---|---|
| `broiler_chicken` | 50 | Output: 2.0 kg/cycle × 0.5 cycles/month = 1.0 kg/month/instance |
| `poultry_feed_kg` demand | 100 kg/month | 4.0 kg/cycle × 0.5 cycles/month × 50 instances |
| `fresh_water_l` demand | 250 L/month | 10.0 L/cycle × 0.5 cycles/month × 50 instances |

## New Graph API: `for_each_node`

```cpp
// Iterate all valid nodes safely (replaces the fragile 0..node_count()+100 pattern).
graph.for_each_node([](homestead::NodeId id, const homestead::NodeVariant& node) {
    std::visit([](const auto& n) {
        // handle EntityInstanceNode, ExternalSourceNode, GoalSinkNode
    }, node);
});
```

## CLI verification

```bash
cmake --preset debug && cmake --build --preset debug
./build/debug/src/cli/homestead_cli \
    --defaults \
    --goals data/sisteminha_goals.json \
    --output plan.json
cat plan.json | python3 -m json.tool | grep -A3 "entity_slug"
```

After this feature, entities in `plan.json` will show non-trivial quantity values instead of `1.0`.
