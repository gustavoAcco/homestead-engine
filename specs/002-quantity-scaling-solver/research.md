# Research: Quantity Scaling Solver

**Feature**: 002-quantity-scaling-solver  
**Date**: 2026-04-08

## §1 — Quantity Calculation Formula

**Decision**: `required_units = ceil(goal_quantity_per_month / (output_per_cycle × cycles_in_month))`

**Rationale**: A homestead cannot operate with fractional entities (half a chicken pen, 0.7 of a fish tank). `ceil` ensures the plan over-provisions rather than under-provisions. The formula uses `cycles_in_month` from `scheduling.cpp` so seasonal and multi-cycle entities are handled correctly and consistently.

**Alternatives considered**:
- `floor()`: under-provisions; shortfall falls to external source. Rejected — planners should see the complete internal answer, not an artificially deflated one.
- `round()`: ambiguous at 0.5 boundary; `ceil` is unambiguous and conservative.
- Fractional quantities: physically meaningless for discrete entities (animals, tanks, plots).

**Representative month**: Use month 0 (January) as the reference month for the initial quantity calculation. If month 0 is inactive (seasonal entity), walk forward to the first active month. If no active month exists, `cycles_in_month = 0` and the entity is skipped (demand falls to external source).

## §2 — Convergence on Quantities

**Decision**: Extend the existing Gauss-Seidel outer loop to track quantity stability. Converge when: (a) no unsatisfied inputs remain, AND (b) no entity quantity changed in the last iteration. Integer quantities guarantee exact equality comparison — no floating-point epsilon needed. Cap at 50 iterations (same as the topology loop, per clarification Q2).

**Rationale**: Circular resource dependencies (chicken → manure → compost → corn → feed → chicken) mean that adding more corn_plot instances generates more compost which could reduce the needed corn_plot instances. A single pass cannot resolve this; fixed-point iteration is the correct approach.

**Iteration data structure**:
```cpp
std::unordered_map<NodeId, double> prev_quantities;
```
Snapshot taken at the start of each outer iteration, compared at the end. If `prev_quantities == current_quantities` (for all entity nodes), converge.

**Divergence handling**: If the cap is reached without convergence, emit a `DiagnosticKind::non_convergent_cycle` diagnostic and return the best-effort result. This is the existing behavior for topology non-convergence; quantities follow the same pattern.

## §3 — Upstream Demand Propagation

**Decision**: The `Demand` queue struct gains a `quantity_per_month` field. When a new entity is instantiated with quantity N, its input demands are enqueued with `demand = pick(inp.quantity_per_cycle, scenario) × N × cycles_in_month(lc, m)`.

**Rationale**: Without this, all upstream entities are still computed at quantity=1. The scaled demand drives `ceil()` for each upstream entity in turn, propagating through the full supply chain.

**Conflict resolution**: When an entity node already exists (reuse path) and a new demand arrives that its current quantity cannot satisfy, the solver recomputes the required quantity from the combined total demand. If the updated quantity exceeds the area or budget constraint, the excess falls to an external source node.

## §4 — External Source Quantities

**Decision**: Set `ResourceFlow.quantity_per_cycle` for flows from `ExternalSourceNode` to the actual monthly shortfall, not `1.0`. No new field on `ExternalSourceNode`.

**Rationale**: The `quantity_per_cycle` on the edge already semantically represents "how much flows per cycle." For external sources, the "cycle" is one month. The shortfall = `total demanded - total supplied internally`.

**Implementation**: When creating or reusing an external source node, compute:
```
shortfall = demand.quantity_per_month - internal_supply_for_resource
```
where `internal_supply_for_resource` is the sum of `quantity_per_cycle × entity_quantity × cycles_in_month` across all entity nodes producing that resource.

## §5 — Graph::for_each_node

**Decision**: Template method on `Graph` with a concept constraint:
```cpp
template<std::invocable<NodeId, const NodeVariant&> Callable>
void for_each_node(Callable&& fn) const;
```
Implemented inline in `graph.hpp` (iterates `nodes_` map).

**Rationale**: The `+100` slop pattern fails silently when nodes are removed (IDs are not contiguous after removal). The map-based iteration is O(n) with no slop, safe regardless of removal history.

**Impact**: All three fragile loops in `analytics.cpp` (`compute_balance_sheet`, `compute_labor_schedule`, `compute_bom`) are replaced. The existing test in `test_backpropagation.cpp` that uses the same pattern is also updated.

## §6 — pick() Deduplication

**Decision**: New internal header `src/solver/detail/pick.hpp` with `homestead::detail::pick()`. Both `backpropagation.cpp` and `analytics.cpp` remove their anonymous-namespace `pick()` and include `detail/pick.hpp`.

**Rationale**: The function is identical in both files. Any future change (e.g., adding a `median` scenario) would need to be made in both places. Single-source removes the divergence risk.

**Scope**: `src/solver/detail/` is under `src/`, not `include/`, so it is a private implementation detail and does not expand the public API surface.

## §7 — Weighted Loop Closure Score

**Decision**: Internal weight map `unordered_map<string, double>` defaulting to 1.0. Not exposed in public API. Formula: `Σ w(r)·min(internal(r), demand(r)) / Σ w(r)·demand(r)`.

**Rationale**: The current formula treats 1 kg of tilapia and 1 kg of water as equal contributors. Weighting by economic value (feature 003) will make the score more meaningful for planning decisions. The infrastructure is added now with default weights to avoid a breaking change later.

**Backward compatibility**: With all weights = 1.0, the formula reduces to the current formula exactly. Existing tests assert on `loop_closure_score` values that will remain correct.

## §8 — No NEEDS CLARIFICATION items

All clarification questions were resolved in `/speckit.clarify` (session 2026-04-08). No unresolved decisions remain.
