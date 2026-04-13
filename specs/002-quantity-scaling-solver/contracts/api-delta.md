# API Delta: Quantity Scaling Solver

**Feature**: 002-quantity-scaling-solver  
**Date**: 2026-04-08  
**Type**: Additive only (no removed or changed signatures)

## New public API

### `homestead::Graph::for_each_node`

**Header**: `include/homestead/graph/graph.hpp`  
**Target**: `homestead::graph`

```cpp
/// Invokes fn(NodeId, const NodeVariant&) for every valid node in the graph.
/// Iteration order is unspecified.
/// Safe after node removal: only visits nodes currently in the graph.
template<std::invocable<NodeId, const NodeVariant&> Callable>
void for_each_node(Callable&& fn) const;
```

**When to use**: Prefer this over `for (NodeId nid = 0; nid < graph.node_count() + N; ...)`.

### `homestead::SolverConfig::max_quantity_iterations`

**Header**: `include/homestead/solver/config.hpp`  
**Target**: `homestead::solver`

```cpp
/// Maximum fixed-point iterations for quantity convergence (distinct from
/// topology convergence). Defaults to 50.
int max_quantity_iterations{50};
```

**Note**: The existing `max_convergence_iterations` field (default 100) is **unchanged**. This new field controls only the quantity-scaling convergence loop introduced in this feature.

---

## Semantics changes (no signature changes)

### `EntityInstanceNode::quantity`

- **Before**: Always `VariableQuantity{1.0, 1.0, 1.0}`
- **After**: `VariableQuantity{N, N, N}` where N = `ceil(demand / output_rate)`, N ≥ 1
- **Impact**: Any code that assumed `quantity.expected == 1.0` must be updated.

### `ResourceFlow::quantity_per_cycle` (from ExternalSourceNode)

- **Before**: Always `VariableQuantity{1.0, 1.0, 1.0}`
- **After**: `VariableQuantity{shortfall, shortfall, shortfall}` where shortfall is the actual monthly unmet demand
- **Impact**: Any code computing external purchase cost using this field now sees real quantities.

---

## Unchanged API

All of the following are **not modified**:

- `homestead::solve()` free function signature
- `homestead::ISolverStrategy::solve()` virtual signature
- `homestead::SolverConfig` existing fields (one new field `max_quantity_iterations` added; existing fields and defaults unchanged)
- `homestead::PlanResult` fields
- `homestead::ProductionGoal` struct
- `homestead::EntityInstanceNode`, `ExternalSourceNode`, `GoalSinkNode` struct fields
- `homestead::ResourceFlow` struct fields
- `homestead::Registry` public methods
- `homestead::Graph` existing methods
