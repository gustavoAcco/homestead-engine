# Feature Specification: Composition-Aware Resource Matching

**Feature Branch**: `004-composition-matching`
**Created**: 2026-04-12
**Status**: Draft

## Overview

The homestead planning engine currently links animal feed and crop fertilizer by
naming exact resource slugs (e.g., "broiler chicken eats poultry_feed_kg",
"corn bed consumes mature_compost_kg"). This rigid wiring means that adding any
new feed crop requires editing every animal that could potentially eat it, and
resources with known nutritional content (such as fish tank water, rich in
nitrogen and phosphorus) are never automatically routed to crop beds because no
explicit link exists.

This feature makes resource routing emerge from first principles: entities declare
*what chemical or nutritional amounts they need*, and the solver discovers which
available resources can satisfy those needs based on each resource's known
composition. A chicken declares a protein requirement; a corn bed declares an
N/P/K requirement; the solver connects them to whatever is already flowing in the
plan that can fill those needs. The registry becomes open rather than brittle,
and closed-loop opportunities that were previously invisible become visible
automatically.

## Clarifications

### Session 2026-04-12

- Q: Feature 003 NutrientBalance diagnostics after composition routing — if Feature 004 routes sufficient manure to crops, should Feature 003's `nutrient_deficit` diagnostics still fire? → A: The two systems are independent. `nutrient_deficit` (Feature 003, post-solve analytics) and `composition_gap` (Feature 004, solve-time routing) coexist and may report the same deficit from different analytical angles. Feature 003 is not updated by Feature 004 routing.
- Q: Loop closure score with mixed units — how should energy (kcal) flows be weighted relative to gram-based composition flows (protein_g, N_g, P_g, K_g)? → A: Energy (kcal) flows are excluded from the loop closure score. Only gram-based composition flows count toward internal supply in the score numerator.
- Q: SC-004 regression safety scope — does "all existing tests pass" hold after registry migration removes slug-based fertilizer inputs from entities? → A: SC-004 means no *unintended* regressions. Tests tied to migrated entities (e.g., tests that expect `compost_bin` in the graph for a corn goal, which was pulled in by the now-removed `mature_compost_kg` slug input) are updated to reflect the new correct behavior. The migration does not keep both slug and composition inputs simultaneously.

## User Scenarios & Testing

### User Story 1 - Animal Feed from Available Crops (Priority: P1)

A homestead planner sets goals for both poultry and grain crops. Currently the
solver independently sizes the grain patch and the chicken coop with no
nutritional connection between them. After this feature, the solver recognises
that the grain already flowing in the plan contains the protein and energy the
chickens need, routes it to meet their nutritional requirements, and reduces or
eliminates external feed purchases.

**Why this priority**: Feed is the largest recurring cost and the biggest
loop-closure opportunity on a homestead. Automatic feed routing is the most
tangible immediate value of the feature.

**Independent Test**: Can be fully tested with a plan containing broiler
chickens plus grain crops; verify that protein and energy requirements are met
from internal grain production and that no external protein purchase appears in
the gap report when grain supply is sufficient.

**Acceptance Scenarios**:

1. **Given** a plan with broiler chicken and corn plot goals, **When** the
   solver runs, **Then** the corn grain already produced internally is routed to
   meet the chickens' protein and energy requirements without an external feed
   purchase.
2. **Given** a plan where internal grain is insufficient, **When** the solver
   runs, **Then** the remaining protein shortfall appears in the gap report as
   an external elemental purchase, not as a generic feed slug.
3. **Given** a plan with no grain or feed crops, **When** the solver runs,
   **Then** the full protein and energy requirement appears as an external
   purchase in the gap report.

---

### User Story 2 - Crop Fertilization from Available Manure and Compost (Priority: P2)

A planner has chickens (producing manure) and crop beds. Currently the solver
does not route chicken manure to crop beds unless the crop explicitly lists
"chicken_manure_kg" as an input. After this feature, crops declare N/P/K
requirements per square metre, and the solver automatically routes any available
manure or compost to meet those requirements, sized correctly for the crop area.

**Why this priority**: Fertilizer routing closes the most important nutrient
loop on a homestead and directly improves the loop closure score — the key
health metric of a plan.

**Independent Test**: Can be fully tested with a plan containing chickens and
corn crop beds; verify that chicken manure is routed to fertilize the beds, the
composition_gap diagnostic is absent when supply is sufficient, and the loop
closure score improves relative to a plan without composition matching.

**Acceptance Scenarios**:

1. **Given** a plan with chickens and corn bed goals, **When** the solver runs,
   **Then** chicken manure produced internally is routed to meet the corn
   beds' N/P/K requirements and no composition_gap diagnostic is emitted for
   those elements.
2. **Given** a plan where only a fish tank is present (no manure), **When** the
   solver runs, **Then** the nitrogen and phosphorus from tank water is routed
   to crop beds even though no explicit link between the fish tank and crop
   beds was declared.
3. **Given** a plan where fertilizer supply covers only part of the N/P/K
   demand, **When** the solver runs, **Then** a composition_gap diagnostic is
   emitted with the exact per-element shortfall, and the gap report shows the
   elemental external purchases.

---

### User Story 3 - Registry Stays Correct Without Manual Wiring (Priority: P3)

When a new crop that can serve as animal feed is added to the registry, it works
immediately in all plans that include it alongside animals — no entity editing
required. The nutritional composition on the resource and the nutritional
requirement on the animal entity are all that is needed.

**Why this priority**: This is the long-term maintainability win. The registry
becomes declarative and extensible without combinatorial wiring.

**Independent Test**: Add a new resource with protein and energy composition
values to a plan alongside an animal entity that has composition requirements;
verify the solver routes it automatically without any changes to the animal
entity.

**Acceptance Scenarios**:

1. **Given** a new feed resource is added to the registry with protein and
   energy composition values, **When** it is included in a plan alongside
   poultry, **Then** the solver routes it toward meeting the poultry's
   nutritional requirements without any changes to the poultry entity.
2. **Given** an animal entity has composition requirements declared, **When** no
   matching resource is present in the plan, **Then** the full requirement
   appears as an external purchase — no crash, no silent omission.

---

### User Story 4 - Existing Plans Unchanged (Priority: P4)

Any plan that does not use composition requirements must produce results
identical to today's solver output. This feature is a strict addition; no
existing plan or test should regress.

**Why this priority**: Regression safety is non-negotiable. All existing tests
must continue to pass.

**Independent Test**: Run all existing tests; verify every test that was passing
before this feature continues to pass with identical output values.

**Acceptance Scenarios**:

1. **Given** a plan where all entities use only slug-based inputs and none have
   composition requirements, **When** the solver runs, **Then** entity
   quantities, balance sheet, and loop closure score are identical to the
   current solver output.

---

### Edge Cases

- What happens when a resource satisfies some but not all required elements
  (e.g., tank water has N and P but no K)? Each element is tracked
  independently; only unmet elements generate external purchases.
- What happens when two resources both partially satisfy the same requirement?
  Greedy allocation fills from the highest loop-closure-contribution source
  first; the second source covers the remainder.
- What happens when a resource is already fully consumed by slug-based inputs
  before the composition pass runs? The composition pass sees net available
  quantity after slug-based consumption and cannot over-allocate.
- What happens when the same resource satisfies N, P, and K simultaneously
  (e.g., chicken manure)? All elements it contributes are counted toward their
  respective requirements when that resource is allocated.
- What happens when a crop entity has both slug-based inputs and composition
  requirements? Slug-based inputs are resolved first; composition matching
  covers the separately declared fertilization requirements.

## Requirements

### Functional Requirements

- **FR-001**: Entities MUST be able to declare nutritional or chemical
  requirements (protein, energy, N, P, K) without naming the specific resource
  that will supply them.
- **FR-002**: Crop entities MUST be able to declare fertilization requirements
  per unit of area per cycle (N, P, K in grams), scaled at runtime by the
  entity's planted area and instance count.
- **FR-003**: The solver MUST match available resources to declared requirements
  based on chemical composition, without requiring explicit slug links between
  the resource and the consumer entity.
- **FR-004**: The matching strategy MUST be greedy: allocate from the internal
  source with the highest loop-closure contribution first, then the next, until
  the requirement is met or internal sources are exhausted.
- **FR-005**: Any requirement that cannot be met from internal sources MUST be
  recorded as an external elemental purchase, visible in the gap report.
- **FR-006**: A composition_gap diagnostic MUST be emitted whenever any
  elemental requirement is purchased externally, naming the element, the
  shortfall quantity, and the entity with unmet demand.
- **FR-007**: The composition matching pass MUST run after the slug-based solver
  pass and MUST NOT add new entity instances — it only routes flows between
  already-instantiated entities and external sources.
- **FR-008**: A plan where no entity declares composition requirements MUST
  produce output identical to the current solver.
- **FR-009**: Resource composition keys in the registry MUST use canonical
  names expressed in grams per unit of resource: N_g, P_g, K_g (plant
  nutrients), protein_g (crude protein), energy_kcal (metabolisable energy),
  organic_matter_g (organic matter for soil amendment quality).
- **FR-010**: The balance sheet for each resource MUST record how much of each
  chemical element was routed through that resource to meet composition demands.
- **FR-011**: The loop closure score MUST account for gram-based composition-
  matched internal flows (protein_g, N_g, P_g, K_g, organic_matter_g), so a
  plan that internally satisfies animal feed or crop fertilization scores higher
  than one that purchases those inputs externally. Energy flows (energy_kcal)
  are excluded from the score to avoid incommensurable unit aggregation.
- **FR-012**: The default registry MUST be migrated so that poultry, goat,
  and crop entities declare composition requirements instead of slug-based
  feed and fertilizer inputs.
- **FR-013**: Non-substitutable inputs (water volume, labor, biological starters
  such as fish fingerlings, construction materials, and process-specific
  inputs) MUST remain as slug-based inputs unchanged.

### Key Entities

- **CompositionRequirement**: A declaration of the chemical or nutritional
  amounts an entity needs per cycle (e.g., protein in grams, energy in kcal,
  or N/P/K in grams). Declared on animal entities and any entity that can be
  fed or fertilized by composition-matched resources.
- **FertilizationRequirement**: A declaration of nutrient need per unit of
  planted area per cycle (N, P, K in grams per m²). Declared on area-based
  crop entities and scaled by area and instance count at solve time.
- **ChemicalComposition**: The nutrient or chemical profile of a resource,
  expressed in grams per unit of that resource. The matching engine uses this
  to determine how much of a resource satisfies a given requirement.
- **CompositionFlow**: The per-element accounting of how much chemical content
  was routed through a specific resource to meet composition demands. Stored per
  resource in the plan's balance sheet.
- **CompositionGap**: A diagnostic record emitted when an elemental requirement
  is partially or fully unsatisfied internally, reporting the element, the
  shortfall in grams, and the entity that needed it.

## Success Criteria

### Measurable Outcomes

- **SC-001**: A plan with broiler chickens and sufficient grain crops produces
  zero external protein or energy purchases when grain protein content is
  adequate to cover the flock's needs.
- **SC-002**: A plan with crop beds and sufficient chicken manure produces no
  composition_gap diagnostic for N, P, or K when the nutrient supply from
  manure meets or exceeds crop demand.
- **SC-003**: A plan with partial fertilizer supply produces a composition_gap
  diagnostic with the exact per-element shortfall in grams, matching
  hand-calculated expected values.
- **SC-004**: No unintended regressions: all existing tests that are unrelated
  to migrated entities pass without modification. Tests that previously verified
  slug-wired behavior now removed by the registry migration (e.g., expecting
  `compost_bin` in the graph for a corn goal) are updated to reflect the correct
  post-migration behavior.
- **SC-005**: The composition matching pass adds no more than 50 ms overhead to
  the solver on a 20-entity plan (which today completes in under 1000 ms).
- **SC-006**: Tank water nitrogen is automatically routed to crop beds in a plan
  that includes a fish tank and crop goals, without any slug link declared
  between the fish tank and the crop entities.

## Assumptions

- Feature 003 (N-P-K Soil Balance) is merged before this feature, establishing
  the canonical g_per_unit composition key convention on fertilizer resources.
- Feature 002 (Quantity Scaling Solver) is already merged; entity instance
  counts are correctly ceil-computed from demand before composition amounts
  are scaled.
- The NutrientDemand field from Feature 003 is a post-solve read-only analytics
  field. This feature introduces a parallel solve-time mechanism
  (FertilizationRequirement) for routing decisions. Both coexist independently:
  `nutrient_deficit` diagnostics (Feature 003) and `composition_gap` diagnostics
  (Feature 004) may report the same elemental deficit from different analytical
  angles. Feature 003's NutrientBalance computation is not modified by Feature
  004's routing pass.
- Composition matching is a post-slug-expansion pass; it does not change which
  entity instances are added to the graph or their quantities.
- The plan output schema version is bumped to a new minor version to record the
  new composition flow information in the balance sheet.
- No new external libraries are required.
- Protein and energy values for poultry and goat entities will be sourced from
  Embrapa and NRC nutrition references.
