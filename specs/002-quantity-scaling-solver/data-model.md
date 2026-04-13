# Data Model: Quantity Scaling Solver

**Feature**: 002-quantity-scaling-solver  
**Date**: 2026-04-08

## Overview

No new persistent data structures are introduced. This feature changes the **computed values** written into existing structures during a solver run. All changes are in `homestead::solver` internals; `homestead::core` and `homestead::serialization` are unmodified.

---

## Modified Structures

### `EntityInstanceNode` (existing — `include/homestead/graph/node.hpp`)

No field additions. Semantics change:

| Field | Type | Before | After |
|---|---|---|---|
| `quantity` | `VariableQuantity` | Always `{1.0, 1.0, 1.0}` | `{N, N, N}` where `N = ceil(demand / output_rate)` and `N ≥ 1` |

All three triple points (`min`, `expected`, `max`) are set to the same integer-valued `double` for the configured scenario. The API is unchanged; callers reading `quantity.expected` see the correct count.

**Validation rules**:
- `quantity.expected ≥ 1.0` (always at least one unit)
- `quantity.expected` is always a whole number (e.g., `25.0`, not `24.7`)
- All three points equal: `quantity.min == quantity.expected == quantity.max`

### `ResourceFlow` (existing — `include/homestead/graph/edge.hpp`)

No field additions. Semantics change for flows from `ExternalSourceNode`:

| Flow type | `quantity_per_cycle` before | `quantity_per_cycle` after |
|---|---|---|
| From `EntityInstanceNode` | `pick(output.quantity_per_cycle, scenario)` | Same (unchanged) |
| From `ExternalSourceNode` | `VariableQuantity{1.0}` | `VariableQuantity{shortfall}` where `shortfall = monthly demand − internal supply` |

---

## New Internal Structures

### `Demand` (internal — `src/solver/backpropagation.cpp`)

Gains a new field. This is a private implementation detail; it is not in any public header.

```
Before:
  Demand { consumer_id: NodeId, resource_slug: string }

After:
  Demand { consumer_id: NodeId, resource_slug: string, quantity_per_month: double }
```

Goal sinks seed the queue with `quantity_per_month = pick(goal.quantity_per_month, scenario)`. Upstream inputs are enqueued with `quantity_per_month = pick(inp.quantity_per_cycle, scenario) × entity_quantity × cycles_in_month(lc, representative_month)`.

### `detail::pick` (new internal header — `src/solver/detail/pick.hpp`)

Not a data structure; a free function. See research.md §6.

---

## New Public API

### `Graph::for_each_node` (new method — `include/homestead/graph/graph.hpp`)

```cpp
template<std::invocable<NodeId, const NodeVariant&> Callable>
void for_each_node(Callable&& fn) const;
```

- Iterates all valid nodes in the graph.
- Order: unspecified (reflects internal `unordered_map` iteration order).
- Callable receives `(NodeId id, const NodeVariant& node)`.
- Safe after node removal: only visits nodes currently in the graph.
- Inline implementation in `graph.hpp` (no separate `.cpp` entry needed).

---

## Unchanged Structures

The following structures are **not modified** by this feature:

| Structure | Location | Reason |
|---|---|---|
| `ExternalSourceNode` | `graph/node.hpp` | Quantity carried on edge, not node |
| `GoalSinkNode` | `graph/node.hpp` | Unchanged |
| `ResourceFlowSpec` | `core/flow.hpp` | Source of truth for output rates; read-only here |
| `Entity` / `Registry` | `core/registry.hpp` | Domain model; untouched |
| `PlanResult` | `solver/result.hpp` | Existing fields populated correctly |
| `SolverConfig` | `solver/config.hpp` | Weight map deferred to feature 003 |
| `ISolverStrategy` | `solver/strategy.hpp` | Interface unchanged |
| `ResourceBalance` | `solver/result.hpp` | Computed values change, structure unchanged |

---

## Lifecycle / State Transitions

No state machine changes. The solver remains a pure function: `solve(goals, registry, config) → PlanResult`. The quantity convergence loop is an internal iteration; callers observe only the final converged state.

---

## Entity Relationship Summary

```
ProductionGoal
  └── seeds Demand{quantity_per_month}
          └── resolved to EntityInstanceNode{quantity = ceil(demand/rate)}
                  └── enqueues Demand{scaled_quantity} for each input
                  └── connects via ResourceFlow{quantity_per_cycle = output_rate × quantity}
          └── or resolved to ExternalSourceNode
                  └── connects via ResourceFlow{quantity_per_cycle = shortfall}
```
