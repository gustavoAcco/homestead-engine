# Feature Specification: homestead-engine Core Library

**Feature Branch**: `001-homestead-engine-core`
**Created**: 2026-04-07
**Status**: Draft

## Clarifications

### Session 2026-04-07

- Q: When multiple registry entities can produce a needed resource and none is yet in the plan, which is added first? → A: The entity with the highest loop-closure contribution — the one that reuses the most resources already produced by entities in the plan.
- Q: What does the solver return when a plan is only partially satisfiable (area exceeded, missing producers, non-convergence)? → A: Always returns a partial plan plus a structured diagnostics list (unsatisfied goals, exceeded constraints, non-convergent loops) — the solver never aborts.
- Q: What time granularity does the resource balance sheet use? → A: Monthly breakdown (12 rows per resource) plus an annual total — consistent with the labor schedule granularity.
- Q: What format does the JSON schema version field use? → A: Semantic version string — `"version": "1.0.0"`, with MAJOR bump for breaking schema changes, consistent with the project's SemVer governance policy.
- Q: Does the Registry validate resource slug references at registration time or later? → A: Eager — validate at registration time and reject invalid entries immediately.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Define and Query Resources and Entities (Priority: P1)

A developer building a planning tool needs to define the building blocks of a
homestead model: resources (what flows) and entities (what transforms resources).
They load the default tropical-agriculture registry, inspect its contents, and
extend it with custom resources and entities specific to their project without
modifying the defaults.

**Why this priority**: All other features depend on a functioning registry. A
developer cannot construct a graph, run the solver, or serialize anything without
first having resources and entities defined. This is the minimum viable foundation.

**Independent Test**: Can be fully tested by loading the default registry,
querying a known resource (e.g., "chicken_manure_kg"), defining a custom entity,
registering it, and verifying the registry returns it correctly — without any graph
or solver involved.

**Acceptance Scenarios**:

1. **Given** the default registry is loaded, **When** a developer queries a resource
   by slug, **Then** the resource is returned with its full property profile
   (composition, nutritional data, physical properties, category).
2. **Given** a developer defines a custom resource with a unique slug, **When** they
   register it, **Then** the registry returns it on subsequent queries and it does not
   overwrite any default resource with a different slug.
3. **Given** a developer defines a custom entity with input/output flows and a
   lifecycle, **When** they register it, **Then** the entity is retrievable and its
   quantities carry the (min, expected, max) triple on every input and output.
4. **Given** a developer attempts to register an entity referencing an unknown resource
   slug, **Then** the registry immediately rejects the registration with a descriptive
   error identifying the unknown slug — the entity is not added to the registry.
5. **Given** the default registry, **When** a developer overrides a default entity by
   registering a replacement with the same slug, **Then** subsequent queries return the
   override, not the original.

---

### User Story 2 - Build and Inspect a Resource-Flow Graph (Priority: P2)

A developer needs to construct a concrete homestead model as a directed graph of
entity instances connected by resource flows. They instantiate entities from the
registry (e.g., "my_chicken_coop" as 50 broiler chickens), connect them to each other
where one entity's output feeds another's input, and add external source nodes for
resources that must be purchased and sink nodes for desired outputs. They can then
inspect the graph to verify connectivity and detect structural problems before
attempting to solve.

**Why this priority**: The graph is the intermediate representation that feeds the
solver and the serializer. A developer must be able to build and validate a graph
independently, because it is also a useful deliverable in its own right (e.g., for
visualization tools).

**Independent Test**: Can be fully tested by constructing a three-node graph
(external source → entity instance → output sink), verifying edges are correctly
directed, querying neighbors, and confirming that a deliberately introduced cycle
(e.g., A → B → A) is detected and reported — without invoking the solver.

**Acceptance Scenarios**:

1. **Given** two entity instances where one produces a resource consumed by the other,
   **When** a developer connects them with a resource-flow edge, **Then** the graph
   reports the correct producer and consumer for that resource.
2. **Given** a graph with a circular dependency (e.g., chicken → manure → compost →
   corn → feed → chicken), **When** the developer queries for cycles, **Then** the
   graph correctly identifies and enumerates the cycle rather than treating it as an
   error.
3. **Given** an entity instance with a resource requirement that has no connected
   producer, **When** the developer queries unsatisfied inputs, **Then** those inputs
   are listed as gaps.
4. **Given** a graph node representing an external purchase, **When** the developer
   sets a per-unit cost, **Then** the cost is retrievable from the node.
5. **Given** a valid graph, **When** a developer removes an entity instance, **Then**
   all edges connected to that instance are also removed and the graph remains
   internally consistent.

---

### User Story 3 - Solve Backwards from Production Goals (Priority: P3)

A developer specifies what the homestead must produce (e.g., "50 kg chicken meat per
month, 200 eggs per month, 30 heads of lettuce per week") and asks the library to work
backwards to determine all entity instances, quantities, and scheduling needed to meet
those goals. The solver closes resource loops wherever possible (using internal
production rather than external purchases) and marks remaining gaps as external
purchases. The result is a complete, actionable production plan.

**Why this priority**: This is the core value proposition of the library — the
automated planning that would take days to do by hand. Without a working solver, the
library is just a data structure.

**Independent Test**: Can be fully tested by specifying a single goal ("20 kg tilapia
per month"), running the solver against the default registry, and verifying the output
plan includes the expected tilapia tank entity, the correct cycle count, and all
upstream input requirements (fingerlings, feed, water, electricity) listed — either
internally sourced or flagged for purchase.

**Acceptance Scenarios**:

1. **Given** a goal of N kg of a resource per month, **When** the solver runs,
   **Then** the plan contains entity instances whose combined expected output meets or
   exceeds N, accounting for the (min, expected, max) variability of each entity.
2. **Given** a circular dependency in the resource graph (chicken manure → compost →
   corn → feed → chicken), **When** the solver encounters it, **Then** it resolves the
   loop via iterative fixed-point convergence and produces a balanced plan rather than
   looping indefinitely or failing.
3. **Given** a goal that requires a seasonal entity (e.g., lettuce in summer only) and
   a year-round planning horizon, **When** the solver runs, **Then** the plan schedules
   the entity only in its permitted months and flags the gap during off-season months.
4. **Given** an available area constraint supplied by the developer, **When** the
   solver would exceed that area to meet all goals, **Then** it reports which goals
   cannot be fully met within the constraint rather than silently over-allocating area.
5. **Given** a resource that can be produced by multiple entities in the registry
   (e.g., soil nutrients from compost_bin or earthworm_bin), **When** solving, **Then**
   the solver prefers an entity already present in the plan before adding a new one,
   to maximise loop closure.
6. **Given** an input that no registry entity can produce, **When** the solver
   encounters it, **Then** it marks the resource as an external purchase and continues
   solving rather than failing.

---

### User Story 4 - Analyze a Solved Plan (Priority: P4)

After solving, a developer extracts structured analytical outputs from the plan:
a resource balance sheet (internal production vs. external purchase for each
resource), a gap report (percentage closed-loop and monetary cost of external inputs),
a labor schedule (hours per month across the year), an infrastructure bill of
materials (area, construction materials, initial investment), and a loop closure score
(a single 0–100% self-sufficiency metric).

**Why this priority**: These outputs are what library consumers actually present to
end users or use to compare alternative plans. Without them, the solver produces an
uninterpretable internal structure.

**Independent Test**: Can be fully tested by solving a known two-entity plan
(one producing chicken meat, one producing the feed from corn) with pre-calculated
expected values for each analytical output, then asserting each figure matches.

**Acceptance Scenarios**:

1. **Given** a solved plan, **When** the developer requests the resource balance sheet,
   **Then** every resource referenced in the plan appears in the sheet with monthly
   figures (12 rows) for internal production, consumption, and external purchase
   quantities, plus an annual total row for each resource.
2. **Given** a solved plan with some external purchases, **When** the developer
   requests the gap report, **Then** each externally purchased resource shows the
   percentage of total demand met externally and the estimated monetary cost based on
   the source node prices.
3. **Given** a solved plan, **When** the developer requests the labor schedule,
   **Then** the schedule lists labor-hours per month for the full planning horizon,
   broken down by entity instance.
4. **Given** a solved plan, **When** the developer requests the infrastructure bill of
   materials, **Then** the output lists total area (m²), all construction materials as
   a resource quantity list, and estimated initial investment cost.
5. **Given** a fully closed-loop plan (zero external purchases), **When** the loop
   closure score is requested, **Then** the score is 100%. Given a plan entirely
   dependent on external inputs, the score is 0%.

---

### User Story 5 - Serialize and Deserialize Plans and Registries (Priority: P5)

A developer needs to save a registry, a graph, or a solved plan to a file and reload
it later — or share it with another tool (e.g., a REST API or a web front-end). The
serialization format is JSON with a version field so that breaking changes to the
schema can be detected and handled.

**Why this priority**: Persistence is required for any real-world usage of the library.
Without it, every run starts from scratch and no plans can be exchanged between tools.

**Independent Test**: Can be fully tested by serializing a registry with three custom
entities to JSON, deserializing it into a fresh registry object, and verifying all
entities and resources round-trip without data loss.

**Acceptance Scenarios**:

1. **Given** a registry with both default and custom entries, **When** serialized to
   JSON and deserialized, **Then** all entries are present and no data is lost,
   including (min, expected, max) quantity triples.
2. **Given** a JSON file produced by an older schema version, **When** the library
   attempts to deserialize it, **Then** the version field is checked and a descriptive
   error is returned if the schema version is incompatible, rather than silently
   producing corrupted data.
3. **Given** a solved plan, **When** serialized and deserialized, **Then** all
   analytical outputs (balance sheet, gap report, labor schedule, loop closure score)
   can be reconstructed from the deserialized plan without re-running the solver.
4. **Given** a malformed JSON input (missing required fields, wrong types), **When**
   deserialization is attempted, **Then** a descriptive error is returned identifying
   the first structural violation, without crashing.

---

### Edge Cases

- A goal quantity of zero: solver should produce an empty plan for that goal, not fail.
- A circular dependency where the cycle requires more of a resource than the cycle can
  produce (e.g., feed demand exceeds corn yield): solver must detect non-convergence
  and report it rather than looping indefinitely.
- An entity with seasonality constraints that conflict with all months in the planning
  horizon: treated as permanently unavailable; dependent goals are fully external.
- A registry with no entity capable of producing a goal resource: solver marks the
  entire goal as external purchase and continues.
- Overlapping resource slugs between default and custom registries: the custom entry
  wins (explicit override semantics).
- Extremely large plans (10,000+ entity instances): solver must complete within
  acceptable time bounds without memory exhaustion.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The library MUST provide a Registry that stores Resource and Entity
  definitions, supports loading a built-in default dataset, and allows custom
  definitions to be added or used to override defaults by slug. The Registry MUST
  validate all resource slug references eagerly at registration time and MUST reject
  any Entity whose input or output references an unregistered resource slug, returning
  a descriptive error identifying the unknown slug.
- **FR-002**: Every Resource MUST carry a chemical composition map, a nutritional
  profile (for food resources), physical properties (weight, volume, perishability),
  and a category classification.
- **FR-003**: Every Entity MUST declare input requirements and output yields as lists of
  (resource slug, quantity, timing-within-cycle) tuples, where each quantity is a
  (min, expected, max) triple — never a single scalar.
- **FR-004**: Every Entity MUST declare a lifecycle (setup duration, cycle length,
  cycles per year, optional seasonality month mask) and capacity/stocking density.
- **FR-005**: The library MUST provide a directed resource-flow graph where nodes are
  Entity instances or special-purpose source/sink nodes, and edges are labeled with
  the resource being transferred.
- **FR-006**: The graph MUST support cycle detection and enumeration without treating
  cycles as errors, because circular resource flows are the intended design.
- **FR-007**: The library MUST provide a backpropagation solver that accepts a set of
  production goals (resource slug + quantity per planning period) and always returns
  a `PlanResult` object paired with a structured Diagnostics list. The solver MUST NOT
  abort or return a failure result on partial satisfiability — every run produces a
  `PlanResult`, even if some goals are unmet.
- **FR-008**: The solver MUST resolve circular dependencies via iterative fixed-point
  convergence and MUST detect and report non-convergence (runaway loops) within a
  bounded number of iterations. Non-convergent loops MUST be recorded in the
  Diagnostics list with the affected resource cycle identified, and solving continues
  for independent parts of the plan.
- **FR-009**: The solver MUST respect seasonal constraints when scheduling entity
  instances and MUST flag goal shortfalls that occur in months when required entities
  cannot operate.
- **FR-010**: The solver MUST respect an optional area constraint; when the constraint
  would be exceeded, it MUST include the unachievable goals in the Diagnostics list
  rather than over-allocating, and MUST continue solving goals that fit within the
  remaining area.
- **FR-011**: The solver MUST prefer closing resource loops with entities already in
  the plan before instantiating new entities from the registry. When multiple registry
  entities can produce a needed resource and none is present in the plan, the solver
  MUST select the entity with the highest loop-closure contribution — defined as the
  entity whose outputs best match resources already consumed by existing plan entities.
- **FR-012**: The library MUST produce the following analytical outputs from a solved
  plan: resource balance sheet (monthly breakdown per resource plus annual totals),
  gap report with external cost estimation, labor schedule (per month), infrastructure
  bill of materials, and loop closure score (0–100%).
- **FR-013**: The library MUST serialize and deserialize Registry, Graph, and Plan
  objects to and from JSON. Every JSON document MUST include a `"version"` field
  containing a semantic version string (e.g. `"1.0.0"`). A MAJOR version increment
  signals a breaking schema change.
- **FR-014**: Deserialization MUST return a descriptive error when the document's
  `"version"` MAJOR component differs from the library's supported MAJOR version, or
  when required fields are missing or have wrong types. It MUST NOT silently produce
  corrupted objects or crash.
- **FR-015**: The library MUST ship a default Registry populated with common
  tropical/subtropical agriculture entities and resources (at minimum: broiler
  chicken, laying hen, tilapia tank, lettuce bed, compost bin, earthworm bin, corn
  plot, and all resources they reference).

### Key Entities

- **Resource**: A substance, material, or product that flows between entities.
  Identified by a unique slug. Carries composition, nutritional profile, physical
  properties, and category.
- **Entity**: A production or transformation unit. Identified by a unique slug.
  Declares inputs, outputs (each as quantity triples), lifecycle, seasonality, area
  requirements, and infrastructure/labor costs.
- **EntityInstance**: A concrete instantiation of an Entity in a plan, with a
  specific quantity (how many units) and a schedule (which months it operates).
- **ResourceFlow**: A directed connection from one EntityInstance to another,
  labeled with the Resource being transferred and the quantity per cycle.
- **ExternalSourceNode**: A graph node representing a resource that must be purchased
  externally, with an associated per-unit monetary cost.
- **GoalSinkNode**: A graph node representing a desired output to be satisfied by the
  solver.
- **Graph**: The directed graph of EntityInstances and source/sink nodes, connected
  by ResourceFlows.
- **PlanResult**: The solver's output (C++ type: `PlanResult`), containing the Graph,
  the scheduling for all instances, the full set of analytical reports, and an attached
  Diagnostics list.
- **Diagnostics**: A structured list of solver findings attached to every Plan,
  recording unsatisfied goals, exceeded constraints, and non-convergent cycles.
  An empty Diagnostics list means the plan is fully satisfied.
- **Registry**: The store of Resource and Entity definitions. Supports default
  entries, custom additions, and per-slug overrides.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can load the default registry, define a three-entity
  homestead (chicken coop + compost bin + corn plot), specify one production goal,
  and receive a complete plan with all analytical outputs in under 100 ms on a
  standard developer machine.
- **SC-002**: The solver handles plans with 10,000 or more entity instances and
  produces a result in under 1 second on a standard developer machine.
- **SC-003**: Every Resource and Entity from the default registry survives a
  JSON round-trip (serialize → deserialize) with no data loss, verified by equality
  comparison.
- **SC-004**: The solver correctly resolves the canonical circular dependency
  (chicken manure → compost → soil nutrients → corn → feed → chicken) and produces
  a converged, balanced plan — verified against hand-calculated expected values.
- **SC-005**: All public library entry points have unit test coverage. No public
  function is shipped without at least one test exercising its happy path and one
  exercising its primary error path.
- **SC-006**: The library compiles without warnings on GCC 13+, Clang 17+, and MSVC
  latest, with all sanitizers (address, undefined behaviour) clean.
- **SC-007**: The loop closure score for a fully closed-loop plan is 100% and for a
  plan with no internal production is 0%, verified by two dedicated tests.

## Assumptions

- The planning horizon is one calendar year (12 months). Multi-year plans and
  per-day granularity are out of scope for this version.
- Monetary costs are single-currency values (no multi-currency conversion). The
  default currency unit is BRL (Brazilian Real), matching the Sisteminha context, but
  the library stores cost as a plain scalar — the currency label is metadata only.
- The default registry covers tropical/subtropical conditions (Brazil). Users in other
  climates are expected to override entity parameters (yields, seasonality) as needed.
- The solver's primary optimization objective is loop closure (maximising
  self-sufficiency). Cost minimization and area minimization are secondary concerns
  deferred to a future feature.
- Entity definitions in the registry are static (no dynamic parameter changes during
  a solve run). Simulation of evolving entity states over time is out of scope.
- The library is consumed as a compiled C++ dependency, not as a standalone CLI tool.
  The minimal CLI harness (`homestead::cli`) exists only for manual testing and is
  not part of the public API surface.
- Water and labor-hours are modeled as Resources with slugs, so they flow through the
  graph and appear in the balance sheet like any other resource.
