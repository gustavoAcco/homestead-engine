# Feature Specification: Nutrient and Soil Balance Tracking (N-P-K)

**Feature Branch**: `003-npk-soil-balance`  
**Created**: 2026-04-09  
**Status**: Draft  

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Detect nutrient deficits before problems occur (Priority: P1)

A homestead planner runs the solver on a Sisteminha-style layout and wants to
know whether the compost and manure outputs actually supply enough nitrogen,
phosphorus, and potassium for all the crop beds — not just whether the mass-flow
balances. Today the solver says "you have 80 kg of compost" but gives no signal
that the compost's nitrogen content is only 60% of what the lettuce beds demand.

**Why this priority**: Nutrient deficiency is the most common real-world failure
mode in small integrated farms. Identifying it at plan time prevents crop losses;
without it the solver produces misleadingly optimistic plans.

**Independent Test**: Build a plan with a chicken coop, compost bin, and two
lettuce beds. Run the solver. Without any external fertilizer, the diagnostic
output must report a nitrogen deficit with an exact shortfall figure. Adding a
small bag of supplemental fertilizer must make the deficit disappear.

**Acceptance Scenarios**:

1. **Given** a plan with chicken manure → compost → lettuce (realistic
   Sisteminha quantities), **When** the plan result is inspected, **Then**
   `NutrientBalance` values for each of the 12 months are present and the
   `available_n` values reflect the actual nitrogen mass in compost outputs.

2. **Given** available nitrogen is less than demanded nitrogen in any month,
   **When** diagnostics are inspected, **Then** exactly one `nutrient_deficit`
   diagnostic per deficient nutrient-month is emitted, naming the element, the
   month index, the available quantity, the demanded quantity, and the shortfall.

3. **Given** a plan where fertilizer inputs fully cover crop demand within 10%
   tolerance, **When** diagnostics are inspected, **Then** no `nutrient_deficit`
   diagnostics are emitted.

---

### User Story 2 - Understand monthly N-P-K supply and demand (Priority: P2)

A planner wants a month-by-month breakdown of how much N, P, and K is supplied
by fertilizer-producing entities versus demanded by crop entities, so they can
decide when supplemental fertilizer is needed and in what quantity.

**Why this priority**: Nutrient flows are seasonal — compost takes 60–90 days to
mature, and crop cycles don't align evenly. Month-level granularity is necessary
to identify the worst-deficit month, not just an annual average.

**Independent Test**: Load a registry with seasonal crop cycles (e.g., corn only
planted April–June). Verify that `demanded_n` values are zero in months with no
active corn planting and nonzero only in active months.

**Acceptance Scenarios**:

1. **Given** a crop entity with a 3-month growing cycle starting in month 4,
   **When** `NutrientBalance.demanded_n` is read, **Then** months 1–3 and 7–12
   have zero demand while months 4–6 carry the full cycle demand.

2. **Given** a plan serialized to JSON, **When** the JSON is read back,
   **Then** all six `NutrientBalance` monthly arrays (`available_n`,
   `available_p`, `available_k`, `demanded_n`, `demanded_p`, `demanded_k`) are
   present and numerically equal to the originals.

---

### User Story 3 - Registry enriched with real Embrapa nutrient data (Priority: P3)

A researcher using the default registry wants nutrient composition values
(N%, P%, K%) already populated for all organic amendment resources, and wants
crop nutrient-demand figures already populated for all crop entities, so they
can run a meaningful nutrient balance without hand-entering reference data.

**Why this priority**: The feature is only useful if the default registry ships
with authoritative values. Without this, every user must source and enter the
data manually, which is both error-prone and a friction barrier.

**Independent Test**: Load the default registry. For each resource whose id
contains "manure", "compost", or "humus", assert that `N_percent`, `P_percent`,
and `K_percent` are all strictly positive. For each area-based crop entity,
assert that `nutrient_demand` is present with at least `n_g_per_m2_per_cycle > 0`.

**Acceptance Scenarios**:

1. **Given** the default registry is loaded, **When** the `chicken_manure_kg`
   resource is accessed, **Then** its `composition` map contains `N_percent`
   approximately 1.5, `P_percent` approximately 1.2, `K_percent` approximately
   0.8 (Embrapa reference as populated in the default registry, ±0.1).

2. **Given** the default registry is loaded, **When** the lettuce crop entity is
   accessed, **Then** its `nutrient_demand` yields `n_g_per_m2_per_cycle`
   approximately 10.0, `p_g_per_m2_per_cycle` approximately 4.0,
   `k_g_per_m2_per_cycle` approximately 8.0.

---

### Edge Cases

- What happens when a resource has `N_percent = 0` (or `N_ppm = 0`, e.g., water, gravel)?
  The resource contributes zero to nutrient supply; no error or warning is emitted.
- What happens when a resource has some but not all of `N_percent`/`P_percent`/`K_percent`
  (or `N_ppm`/`P_ppm`) set?
  Absent elements default to 0.0. The resource still contributes supply for the
  elements that are present; missing elements simply contribute nothing.
- What happens when a crop entity has no `nutrient_demand` set?
  The entity is silently excluded from demand calculation (demand treated as zero for that entity).
- What happens when an entity produces a fertilizer resource but its output
  quantity is zero for some months?
  `available_n/p/k` for those months is zero; deficit is computed normally.
- What if the solver has not yet been run (empty `PlanResult`)?
  `NutrientBalance` is initialized to all-zero arrays; no diagnostics are emitted.
- What if the same nutrient is supplied by multiple entity types simultaneously?
  All net-output contributions are summed; only the combined total is compared to demand.
- What quantity is used when a fertilizer resource is partially consumed as
  feedstock by another entity (e.g., compost → earthworm bin)?
  Only the net quantity (produced minus consumed within the plan) contributes to
  nutrient supply. Feedstock portions are not double-counted.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST track N, P, and K supply and demand at monthly
  resolution for any solved plan.
- **FR-002**: The system MUST compute available N/P/K from each entity's **net
  output** — the quantity of a fertilizer resource produced minus the quantity of
  that same resource consumed by non-crop entities within the same plan. For solid
  resources this net quantity (kg/month) is multiplied by `N_percent`/`P_percent`/
  `K_percent` / 100 × 1000 to produce grams; for liquid resources the net quantity
  (L/month) is multiplied by `N_ppm`/`P_ppm` / 1000 to produce grams. Resources
  fully consumed as feedstock by intermediate entities contribute zero to nutrient
  supply.
- **FR-003**: The system MUST compute demanded N/P/K per crop entity by
  multiplying entity area (m²) by the nutrient demand per m² per cycle and by
  the prorated fraction of each cycle that falls within the month (cycle days
  overlapping the month ÷ total cycle length). Partial cycles at month
  boundaries contribute proportional demand, not full-cycle demand.
- **FR-004**: The system MUST emit a `nutrient_deficit` diagnostic whenever, in
  any month, demanded N (or P or K) exceeds available N (or P or K). The
  diagnostic MUST identify the specific element, month index, available quantity,
  demanded quantity, and shortfall quantity.
- **FR-005**: `NutrientDemand` MUST be an optional field on `Entity`; entities
  without it are excluded from demand calculation without error.
- **FR-006**: `NutrientBalance` MUST be included in `PlanResult` and MUST be
  fully serializable and deserializable via the existing serialization module
  without data loss. When deserializing a plan file whose schema version is
  below `1.1.0` (i.e., no `NutrientBalance` field present), the system MUST
  zero-initialize all six monthly arrays and succeed without error or warning.
- **FR-007**: The plan result JSON schema version MUST be bumped to `1.1.0` when
  this feature ships. The change is backward-compatible: old 1.0.x readers that
  ignore unknown fields will continue to work; new readers tolerate missing
  `NutrientBalance` by zero-initialization (see FR-006).
- **FR-008**: The nutrient balance computation MUST NOT alter the solver's
  entity-selection or quantity decisions; it is a read-only post-solve
  validation pass.
- **FR-009**: The default registry MUST include nutrient composition values
  (sourced from Embrapa technical references) for all manure, compost,
  vermicompost, digestate, and commercial fertilizer resources. Solid resources
  use `N_percent`/`P_percent`/`K_percent` (% of dry weight); liquid resources
  use `N_ppm`/`P_ppm` (mg/L). The analytics layer handles unit conversion;
  keys must not be renamed if already present, to preserve backward
  compatibility with existing consumers.
- **FR-010**: The default registry MUST include `NutrientDemand` values for all
  **area-based** crop entities: lettuce, corn, tomato, peppers, beans, cassava,
  and any other entities whose quantity is expressed per m². Per-plant entities
  (`banana_plant`, `papaya_plant`, `acerola_plant`) are excluded because
  `NutrientDemand` fields are per-m²-per-cycle and do not apply to per-plant
  quantity models; support for per-plant nutrient demand is deferred to a future
  feature.
- **FR-011**: An integration test MUST model the classic Sisteminha nutrient loop
  (tilapia tank → vegetable beds, chicken coop → manure → compost → crops,
  earthworm bin → vermicompost → vegetable beds) and assert a **fixed,
  predetermined outcome** computed in advance from the Embrapa reference values
  and default registry quantities. The expected per-element, per-month
  supply and demand figures MUST be hand-calculated and hard-coded in the test;
  the test MUST NOT accept both deficit and surplus as valid outcomes.

### Key Entities

- **Resource** (existing): gains `N_percent`/`P_percent`/`K_percent` (solid
  resources, % dry weight) or `N_ppm`/`P_ppm` (liquid resources, mg/L) entries
  in its existing `composition` map. No structural change to the type —
  only data population in the default registry.
- **NutrientDemand** (new): optional value attached to a crop entity capturing
  per-m²-per-cycle demand for N, P, and K. Absent on non-crop entities.
- **NutrientBalance** (new): six month-indexed series (available and demanded for
  each of N, P, K) embedded in `PlanResult`. Carries the complete nutrient
  supply-versus-demand picture for a solved plan.
- **nutrient_deficit diagnostic** (new): a `DiagnosticKind` value emitted when
  any month's demand exceeds supply for a given nutrient element. Carries the
  element name, month, available g, demanded g, and shortfall g.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Loading the default registry and running the solver on the
  Sisteminha integration test produces a `NutrientBalance` with all 12 monthly
  values populated for all six arrays (72 values present, none absent or NaN),
  matching hand-calculated reference figures within floating-point tolerance.
- **SC-002**: When any nutrient is in deficit, exactly one `nutrient_deficit`
  diagnostic per element-month combination is emitted — no duplicates, no
  omissions — and the shortfall figure equals demanded minus available to within
  floating-point rounding (epsilon ≤ 1e-9 g).
- **SC-003**: A `PlanResult` round-tripped through JSON serialization and
  deserialization produces identical `NutrientBalance` values with no data loss.
- **SC-004**: All existing solver tests continue to pass unchanged after this
  feature is added (the nutrient pass does not modify mass-flow results).
- **SC-005**: All new unit and integration tests pass under the sanitizer build
  preset with zero memory errors and zero undefined-behaviour reports.
- **SC-006**: The default registry contains positive `N_percent`, `P_percent`,
  and `K_percent` (or `N_ppm`/`P_ppm` for liquid resources) for every resource
  whose identifier indicates an organic amendment, and a positive
  `NutrientDemand.n_g_per_m2_per_cycle` for every area-based crop entity.

## Clarifications

### Session 2026-04-09

- Q: How should cycle demand be allocated to months when a cycle straddles a month boundary? → A: Prorated — demand allocated proportionally to the fraction of cycle days that fall within each month.
- Q: When deserializing a 1.0.x plan file (no NutrientBalance field), what should happen? → A: Zero-initialize all six monthly arrays and succeed silently — no error, no warning.
- Q: If a resource has some but not all of N_pct/P_pct/K_pct set, how are missing elements handled? → A: Default to 0.0; the resource contributes supply for present elements only.
- Q: Should available nutrient supply be computed from gross production or net output (minus intra-plan consumption)? → A: Net output — quantities consumed as feedstock by other entities are excluded to avoid double-counting.
- Q: Should the Sisteminha integration test accept either deficit or surplus as a valid outcome? → A: No — fixed predetermined outcome only; expected supply/demand figures are hand-calculated from reference data and hard-coded in the test.
- Factual (registry key naming): The registry already uses `N_percent`/`P_percent`/`K_percent` for solids and `N_ppm`/`P_ppm` for liquids. The analytics layer handles both via suffix detection; keys must not be renamed. FR-009 updated accordingly.

## Assumptions

- The nutrient balance is computed after the solver converges and reads the
  solved quantities without modifying them. Making the solver nutrient-aware is
  explicitly out of scope for this feature.
- Cycle demand is prorated per month: each month receives (days of cycle
  overlapping that month ÷ total cycle length) × full-cycle demand. This uses
  scheduling data (cycle start day, cycle length) already computed by the
  solver; no new scheduling logic is required.
- Nutrient composition values are expressed as percent of dry weight. Wet-weight
  correction factors are out of scope; users needing wet-weight accuracy should
  adjust the percentage values in the registry.
- All resources that can contribute to nutrient supply are identified by the
  presence of `N_percent`/`P_percent`/`K_percent` (solids) or `N_ppm`/`P_ppm`
  (liquids) in their `composition` map. No separate "is_fertilizer" flag is
  introduced.
- The 10% tolerance in the Sisteminha integration test accounts for rounding in
  published reference values and cycle-boundary approximations in monthly
  aggregation.
- No new external dependencies are introduced; the feature uses only the C++
  standard library and existing project dependencies.
