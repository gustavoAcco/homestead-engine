# Research: Composition-Aware Resource Matching (004)

## Decision 1: Primary allocation key for animal feed matching (Pass 1)

**Decision**: Energy (energy_kcal) is the primary allocation key for Pass 1.
Protein (protein_g) is the secondary constraint.

**Rationale**: Animals eat to satisfy their energy needs (energy drives intake
regulation in monogastrics and ruminants). Protein availability is checked after
energy-based allocation to detect a protein shortfall. This mirrors the N-first
convention in Pass 2 (N is the primary limiting element in tropical soils).

**Alternatives considered**:
- Protein-first: Overshoots energy badly when low-protein/high-energy resources
  (e.g., cassava) are allocated to meet protein — requires much more cassava than
  the animal would actually eat.
- Simultaneous optimisation: LP-solver territory (Feature 006); greedy doesn't
  support multi-objective allocation in this pass.

---

## Decision 2: Per-plant crop entities — fertilization_per_m2 vs composition_requirements

**Decision**: banana_plant, papaya_plant, acerola_plant use `composition_requirements`
(N_g, P_g, K_g per plant per cycle), NOT `fertilization_per_m2`. The solver scales
by instance count (number of plants), not by area × instance count.

**Rationale**: `fertilization_per_m2` is area-scaled at solve time:
`demand = fertilization_per_m2[key] * area_m2 * quantity`. For per-plant entities
(area_m2 is the footprint of one plant, e.g., 4 m² for banana), using area-scaling
would produce `4 * 1 * fertilization_per_m2` per plant — a valid result, but the
composition_requirements approach is cleaner and consistent with Feature 003's
restriction (NutrientDemand was also excluded from per-plant entities).

**How to apply**: Per-plant crop composition requirements are expressed in total grams
of N/P/K per plant per cycle, pre-multiplied by the plant's typical area footprint.

---

## Decision 3: tilapia_tank_5000l fish_feed_kg input

**Decision**: tilapia_tank_5000l keeps its `fish_feed_kg` slug-based input unchanged.
It is NOT migrated to composition_requirements in this feature.

**Rationale**: `fish_feed_kg` is always an external purchase (no entity produces it).
Migrating to composition_requirements would produce the same outcome (elemental
external purchases of protein_g_external and energy_kcal_external) but with more
diagnostic noise and no loop-closure benefit. The spec explicitly says to keep
non-substitutable inputs slug-based.

**Future work**: If a future feature adds fish meal or insect larvae as producers,
the tilapia feed migration can be done then.

---

## Decision 4: Composition_requirements concrete values

Values derived from existing registry feed inputs × NutritionalProfile data
(which are already validated against Embrapa references).

| Entity | cycle_days | protein_g/cycle | energy_kcal/cycle | Derivation |
|---|---|---|---|---|
| broiler_chicken | 60 | 720.0 | 11600.0 | 4.0 kg × (180 g/kg, 2900 kcal/kg) |
| laying_hen | 30 | 21.6 | 348.0 | 0.12 kg × (180 g/kg, 2900 kcal/kg) |
| quail | 30 | 5.04 | 81.2 | 0.028 kg × (180 g/kg, 2900 kcal/kg) |
| goat | 30 | 180.0 | 2700.0 | 1.5 kg × (120 g/kg, 1800 kcal/kg) |
| goat_kids | 150 | 60.0 | 900.0 | 0.5 kg × (120 g/kg, 1800 kcal/kg) |

Per-plant crop N/P/K requirements per plant per cycle (from Embrapa crop nutrition
bulletins — approximate values to be confirmed during implementation):

| Entity | cycle_days | N_g | P_g | K_g |
|---|---|---|---|---|
| banana_plant | 365 | 120.0 | 40.0 | 200.0 |
| papaya_plant | 30 | 5.0 | 2.0 | 7.0 |
| acerola_plant | 30 | 3.3 | 1.5 | 4.0 |

---

## Decision 5: fertilization_per_m2 values for area-based crops

Reuse NutrientDemand values from Feature 003 (same Embrapa source):

| Entity | N_g/m²/cycle | P_g/m²/cycle | K_g/m²/cycle |
|---|---|---|---|
| lettuce_bed_1m2 | 10.0 | 4.0 | 8.0 |
| tomato_bed_1m2 | 15.0 | 6.0 | 12.0 |
| pepper_bed_1m2 | 12.0 | 5.0 | 10.0 |
| corn_plot_1m2 | 8.0 | 3.0 | 6.0 |
| cassava_plot_1m2 | 5.0 | 2.0 | 8.0 |

bean_plot_1m2 is NOT migrated — it already has no mature_compost_kg input (legume,
fixes N). No fertilization_per_m2 added.

---

## Decision 6: Feed resource composition map additions

protein_g and energy_kcal keys are added to the `composition` map for each resource
that can serve as animal feed. Values are in g or kcal per 1 unit of that resource
(1 kg for kg-unit resources). Sourced from existing NutritionalProfile data already
in the registry.

| Resource | protein_g/unit | energy_kcal/unit |
|---|---|---|
| corn_grain_kg | 93.0 | 3580.0 |
| bean_kg | 220.0 | 3400.0 |
| cassava_kg | 14.0 | 1590.0 |
| cassava_leaves_kg | 200.0 | 900.0 |

Note: `poultry_feed_kg` and `goat_feed_kg` are NOT given composition keys — they
remain as slug-named resources (producers keep outputting them), but no entity
declares them as an input after migration. They will become "orphan" resources in
the graph (not consumed by anything), which is valid.

---

## Decision 7: Plan schema version

PLAN_SCHEMA_VERSION: 1.1.0 → 1.2.0 (minor bump; ResourceBalance::composition_routed
is additive; backward-compatible deserialization emits empty map when field absent).

---

## Decision 8: Loop closure score integration for composition flows

Composition-matched flows are tracked in `ResourceBalance::composition_routed`
as `map<element_key, grams_routed>`. These flows represent internal supply of
chemical elements through specific resources.

For the loop closure score: gram-keyed element flows (N_g, P_g, K_g, protein_g,
organic_matter_g) routed through composition matching are added to the internal
supply numerator as their gram quantity (unit: grams). The denominator is also
augmented by the total elemental demand (grams) for those elements.

`energy_kcal` flows are excluded to avoid incommensurable unit aggregation (a
committed clarification decision from the spec).

The score remains in [0.0, 1.0] as before.
