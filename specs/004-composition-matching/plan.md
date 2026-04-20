# Implementation Plan: Composition-Aware Resource Matching

**Branch**: `004-composition-matching` | **Date**: 2026-04-12 | **Spec**: [spec.md](spec.md)

## Summary

Replace slug-based feed and fertilizer inputs on entity definitions with
chemical/nutritional requirement declarations. The solver matches available
resources to those requirements by composition (greedy, loop-closure-first),
so connections emerge automatically. Two post-slug-expansion passes are added
to `BackpropagationSolver::solve()`: Pass 1 routes feed resources to animal
entities; Pass 2 routes fertilizer resources to crop entities. Unmet elemental
demand is purchased as external elemental resources and reported via a new
`composition_gap` diagnostic.

**Depends on**: 002-quantity-scaling (merged), 008-nutrient-soil-balance
(must be merged first — supplies N_g/P_g/K_g keys on fertilizer resources).

## Technical Context

**Language/Version**: C++23 (GCC 13+, Clang 18+, MSVC latest)
**Primary Dependencies**: nlohmann/json 3.11+ (serialization), Catch2 v3.6+ (testing)
**Storage**: JSON files (default_registry.json, PlanResult serialization)
**Testing**: Catch2 v3, ctest presets (debug/release/sanitize)
**Target Platform**: Linux (GCC, Clang), Windows (MSVC) — same as existing CI matrix
**Project Type**: C++ library (homestead::core, homestead::solver, homestead::serialization)
**Performance Goals**: Composition passes add ≤ 50 ms overhead on 20-entity plan (SC-005)
**Constraints**: No new CMake targets; no new external dependencies; sanitize preset must pass
**Scale/Scope**: 20 entities in default registry; O(n²) worst-case for composition scan is acceptable at this scale

## Constitution Check

| Principle | Status | Notes |
|---|---|---|
| I. Modern C++23 Idioms | PASS | unordered_map value semantics; no new exceptions; std::expected preserved |
| II. Separation of Concerns | PASS | New Entity fields in core (no I/O); passes in solver; serialization in serialization |
| III. Testing | PASS | 5 new test cases in test_composition_matching.cpp; round-trip tests in test_roundtrip.cpp |
| IV. Documentation | PASS | Doxygen `///` comments required on new Entity fields and new DiagnosticKind value |
| V. Code Style | PASS | snake_case fields; clang-format/tidy must pass before commit |
| VI. Build System | PASS | No new targets; new source files added to existing CMakeLists targets |
| VII. CI | PASS | Existing CI matrix covers all changes; sanitize run mandatory |
| VIII. Performance | PASS | SC-005: ≤50 ms overhead; passes are O(entities × candidates) — fast at 20 entities |

No complexity violations. No Complexity Tracking table required.

## Project Structure

### Documentation (this feature)

```text
specs/004-composition-matching/
├── plan.md              ← this file
├── research.md          ← Phase 0: nutrition values, allocation decisions
├── data-model.md        ← Phase 1: entity fields, migration table, schema bump
├── quickstart.md        ← Phase 1: 5 integration scenarios
├── contracts/
│   └── public-api.md    ← Phase 1: C++ API contract for new fields and passes
└── tasks.md             ← Phase 2 output (speckit.tasks — not yet created)
```

### Source Code (modified files only)

```text
include/homestead/core/
└── entity.hpp                      ← +composition_requirements, +fertilization_per_m2

include/homestead/solver/
└── result.hpp                      ← +ResourceBalance::composition_routed
                                       +DiagnosticKind::composition_gap

include/homestead/serialization/
└── schema_version.hpp              ← PLAN_SCHEMA_VERSION 1.1.0 → 1.2.0

src/core/
└── registry.cpp                    ← +validate composition_requirements/fertilization_per_m2

src/solver/
├── backpropagation.cpp             ← +run_feed_matching_pass()
│                                      +run_fertilization_matching_pass()
│                                      insert both after slug-expansion, before analytics
└── analytics.cpp                   ← +update loop_closure_score with composition_routed

src/serialization/
└── serialization.cpp               ← +entity composition fields round-trip
                                       +ResourceBalance::composition_routed round-trip
                                       +1.1.x backward-compat: absent → empty map

data/
└── default_registry.json           ← migrate animal/crop entities (see data-model.md)
                                       add protein_g/energy_kcal to feed resources

tests/solver/
└── test_composition_matching.cpp   ← new: 5 test cases (see quickstart.md)

tests/serialization/
└── test_roundtrip.cpp              ← +round-trip for composition_requirements,
                                        fertilization_per_m2, composition_routed
```

## Implementation Notes

### Pass 1 implementation sketch (backpropagation.cpp)

```cpp
// Called after slug-based demand loop converges, before populate_plan_result().
void run_feed_matching_pass(Graph& g, const Registry& registry,
                            const SolverConfig& config,
                            std::unordered_map<std::string, NodeId>& external_nodes,
                            PlanResult& result) {
    // 1. Collect all resources already in graph with their monthly production.
    // 2. For each EntityInstanceNode with non-empty composition_requirements:
    //    a. Compute scaled demand (qty × cpm for representative month).
    //    b. Sort candidate resources by output count (loop-closure proxy).
    //    c. Greedy: primary=energy_kcal, secondary=protein_g.
    //    d. For unmet remainder: get_or_create_external("energy_kcal_external", ...)
    //       and emit DiagnosticKind::composition_gap.
    // 3. Update ResourceBalance::composition_routed for each allocated resource.
}
```

### Pass 2 implementation sketch (backpropagation.cpp)

```cpp
void run_fertilization_matching_pass(Graph& g, const Registry& registry,
                                     const SolverConfig& config,
                                     std::unordered_map<std::string, NodeId>& external_nodes,
                                     PlanResult& result) {
    // 1. For each EntityInstanceNode with non-empty fertilization_per_m2:
    //    a. Demand = per_m2[key] × area_m2 × qty × cpm.
    //    b. Candidates: resources with N_g, P_g, or K_g in composition.
    //    c. Primary=N_g; allocate units to meet N demand; credit P and K.
    //    d. Unmet per-element → ExternalSourceNode + composition_gap diagnostic.
}
```

### Loop closure score update (analytics.cpp)

```cpp
// In compute_loop_closure_score(), after mass-flow loop:
for (const auto& bal : balance_sheet) {
    for (const auto& [key, grams] : bal.composition_routed) {
        if (key == "energy_kcal") { continue; }  // excluded per Q2 clarification
        double demand_g = /* total grams of this element demanded by plan */ ...;
        total_demand += demand_g;
        internal_supply += std::min(grams, demand_g);
    }
}
```

### External composition node slugs

Synthetic slugs for external composition purchases follow the pattern
`"{element_key}_external"`:
- `"protein_g_external"`, `"energy_kcal_external"`
- `"N_g_external"`, `"P_g_external"`, `"K_g_external"`

These slugs are NOT registered in the Registry. They are solver-generated.
The gap report and diagnostics display them as-is.

### Test migration note (SC-004 scope)

Any existing test that exercised an entity with a now-migrated input (e.g.,
corn_plot_1m2 pulling in compost_bin via mature_compost_kg) will see different
graph topology after the registry migration. Those tests must be updated to
reflect the new correct behavior. This is expected and not a regression — it
is the intended result of the migration. Tests unrelated to migrated entities
are unaffected.

## Dependency Reminder

Feature 008 (nutrient-soil-balance key canonicalization) must be merged before
this feature can be fully implemented. Specifically:
- Fertilizer resources need N_g, P_g, K_g in their `composition` maps.
- nutrient_water_l needs N_g, P_g in its `composition` map.
- Without those keys, Pass 2 finds no candidates and all fertilization demands
  become external purchases (feature works but loop-closure benefit is zero).

Feature 004 implementation can proceed on the data model changes, solver pass
scaffolding, and test stubs before Feature 008 lands. Full integration tests
(Scenarios 2, 3, 5 in quickstart.md) require Feature 008 to be complete.
