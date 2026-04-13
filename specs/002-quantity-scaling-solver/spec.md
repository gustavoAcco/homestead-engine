# Feature Specification: Quantity Scaling Solver

**Feature Branch**: `002-quantity-scaling-solver`  
**Created**: 2026-04-08  
**Status**: Draft  
**Input**: User description: "Quantity Scaling Solver — fix backpropagation solver to compute meaningful entity counts from production goals"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Goal-Driven Entity Counts (Priority: P1)

A homestead planner provides production goals (e.g., 50 kg of broiler meat per month) and expects the engine to return a realistic number of entities required to meet those goals (e.g., 25 broiler chicken units), not a hardcoded value of 1.

**Why this priority**: This is the foundational correctness fix. Without it, every quantity in the plan output is meaningless. All downstream features (economic analysis, phased planning, sensitivity analysis) depend on correct quantities.

**Independent Test**: Can be fully tested by calling `solve()` with a single-goal scenario and asserting the returned entity instance quantity matches the hand-calculated expected count within 5%.

**Acceptance Scenarios**:

1. **Given** a goal of 50 kg broiler_meat_kg per month and the default registry, **When** the solver runs, **Then** the plan contains a broiler_chicken entity instance with a quantity that, when multiplied by its output rate, satisfies the goal.
2. **Given** a goal of 200 eggs per month, **When** the solver runs, **Then** the plan contains a laying_hen entity instance with a quantity that satisfies the monthly egg demand.
3. **Given** an entity that cannot fully satisfy a goal through internal production, **When** the solver runs, **Then** the shortfall is reported as an external source purchase with a quantity equal to the unmet demand.

---

### User Story 2 - Upstream Input Scaling (Priority: P2)

When the planner requires N units of a producing entity, all of its input demands (feed, water, labor, seed) scale proportionally by N. Those scaled demands then drive the required quantity of upstream entities.

**Why this priority**: Without upstream scaling, the plan output misrepresents resource requirements — a farm plan for 25 chickens would still show inputs only for 1 chicken.

**Independent Test**: Can be fully tested by asserting that for a plan with a known number of broiler_chicken units, the calculated feed demand equals units × per-unit-feed-rate.

**Acceptance Scenarios**:

1. **Given** 25 broiler_chicken units are required, **When** the solver computes inputs, **Then** the total feed demand equals 25 times the per-unit feed flow.
2. **Given** 25 broiler_chicken units whose combined feed demand exceeds what one corn_plot produces, **When** the solver runs, **Then** the plan contains multiple corn_plot units sufficient to cover the demand.
3. **Given** a circular dependency (chicken manure → compost → corn → feed → chicken), **When** the solver iterates, **Then** it converges to a stable set of quantities without infinite looping.

---

### User Story 3 - Accurate Self-Sufficiency Score (Priority: P3)

The loop closure score (how self-sufficient the homestead is) is computed from actual scaled resource flows, not from placeholder quantities. The score meaningfully reflects whether the homestead meets its goals from internal production.

**Why this priority**: Planners use the loop closure score to compare configurations. A score computed from quantity=1 across all entities is not comparable or actionable.

**Independent Test**: Can be fully tested by asserting that a plan meeting 100% of goals internally scores 1.0, and a plan that imports everything externally scores 0.0.

**Acceptance Scenarios**:

1. **Given** a plan where all goals are met entirely by internal entities, **When** the loop closure score is computed, **Then** the score is 1.0 (within floating-point tolerance).
2. **Given** a plan where all goals require external purchase, **When** the loop closure score is computed, **Then** the score is 0.0.
3. **Given** a partial plan, **When** the loop closure score is computed, **Then** the score reflects the fraction of demand satisfied internally.

---

### Edge Cases

- What happens when a goal quantity is zero? The solver should produce no entity instances for that goal.
- What happens when no internal entity can produce a required resource? The shortfall must be reflected as 100% external purchase with the correct quantity.
- What happens when the fixed-point iteration does not converge within the configured limit? The solver must report a diagnostic warning and return the best-effort result.
- What happens when an entity's output rate is zero for a given lifecycle/schedule? Division by zero must be guarded; the entity is skipped silently and the solver falls through to the next capable producer. If no capable producer exists, the full demand is assigned to an external source node.
- What happens when multiple entities can produce the same resource? The first-registered producer is selected and receives the full demand; no demand splitting occurs. Entity selection logic is unchanged from feature 001.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The solver MUST compute entity instance quantities as `ceil(goal_quantity_per_month / (output_quantity_per_cycle × cycles_in_month))`, using the scenario point from `SolverConfig`. Entity quantities are always whole (integer) numbers; fractional values are never stored or returned.
- **FR-002**: The solver MUST scale each entity's input demands proportionally to the entity's computed instance quantity before propagating upstream.
- **FR-003**: The solver MUST re-evaluate quantities in subsequent iterations and increase them if downstream demands grow (fixed-point convergence on quantities).
- **FR-004**: The fixed-point loop MUST terminate when all entity instance quantities are identical between consecutive iterations (no change), or when a dedicated quantity iteration limit is reached (new `max_quantity_iterations` field, default 50). The existing `max_convergence_iterations` field and its default of 100 MUST NOT be changed. Since quantities are integers, convergence means exact equality — no floating-point epsilon is needed.
- **FR-005**: External source nodes MUST carry a quantity equal to the total unmet monthly demand for the resource they supply.
- **FR-006**: The loop closure score formula MUST support per-resource weighting, defaulting to weight=1.0 for all resources (preserving current behavior).
- **FR-007**: The quantity-picking helper (selecting min/expected/max from a triple based on scenario) MUST exist in exactly one place within the solver module.
- **FR-008**: Graph node iteration in analytics and scheduling MUST use a safe enumeration method that only visits valid, existing node IDs.
- **FR-009**: All public API signatures (solver strategy interface, plan result structure, solve function, solver configuration) MUST remain unchanged.
- **FR-010**: All existing tests MUST continue to pass; quantity-specific assertions MAY be updated to reflect correctly computed values.

### Key Entities

- **Entity Instance**: Represents a required quantity of a specific production unit in the plan; its quantity field must reflect the computed count needed to satisfy downstream demand.
- **External Source**: Represents external purchases; its quantity must reflect the monthly shortfall that cannot be met internally.
- **Production Goal**: The input demand specification; its monthly quantity drives the root-level quantity calculation.
- **Variable Quantity**: A (min, expected, max) triple; the solver selects one point based on the configured scenario for all calculations.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A plan for "50 kg broiler meat per month" against the default registry returns a broiler_chicken entity count within 5% of the hand-calculated expected value.
- **SC-002**: For any solved plan, the sum of all incoming resource flows to each goal sink equals or exceeds the goal quantity (property invariant holds for all generated scenarios).
- **SC-003**: The loop closure score for a plan where all demand is met internally equals 1.0 (within standard double-precision floating-point tolerance).
- **SC-004**: The solver converges (or reports a non-convergence diagnostic) within the configured iteration limit for all valid inputs, including circular dependency graphs.
- **SC-005**: All existing tests pass without regression after this feature is implemented.

## Clarifications

### Session 2026-04-08

- Q: Should entity quantities be stored as whole numbers (ceil to integer) or kept fractional internally and rounded only for display? → A: Always ceil to integer; entity quantities are always whole numbers. A homestead cannot operate with a fraction of an animal or a partial plot.
- Q: What is the default iteration cap for the quantity fixed-point loop? → A: 50 iterations, consistent with the existing topology convergence loop in backpropagation.cpp.
- Q: When multiple entities can supply the same input resource, how is demand distributed? → A: First-registered producer takes all demand; no splitting. Selection logic is unchanged from feature 001; multi-producer optimization is deferred to feature 006.
- Q: What does the convergence epsilon measure, given that entity quantities are integers? → A: Convergence is exact equality between consecutive iterations (no change in any integer quantity); no floating-point epsilon needed. The iteration cap provides the upper bound.
- Q: When an entity's output rate is zero for a given lifecycle/schedule, should it error, warn, or skip silently? → A: Skip silently and fall through to the next capable producer; if none exists, assign demand to an external source node.

## Assumptions

- The `cycles_in_month()` scheduling helper already computes the correct value for a given entity lifecycle and calendar month; this feature reuses it without modification.
- Entity selection logic (which producer is chosen when multiple entities can satisfy a resource) is not changed by this feature; only the quantity of the selected producer is affected.
- The scenario configuration (optimistic/expected/pessimistic) already exists in the solver configuration and is populated by callers; this feature uses it without extending it.
- Circular resource dependencies (e.g., chicken → manure → compost → corn → feed → chicken) exist in the default registry; the fixed-point iteration structure is extended, not replaced.
- Economic weighting of the loop closure score is deferred to the economic analysis feature; this feature only adds the weighting infrastructure with default weight=1.0.
