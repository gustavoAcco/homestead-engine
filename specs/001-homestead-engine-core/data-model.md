# Data Model: homestead-engine Core Library

**Branch**: `001-homestead-engine-core` | **Date**: 2026-04-07

All types use value semantics (copyable, movable). No raw pointers. No exceptions —
fallible operations return `std::expected<T, E>`.

---

## Module: homestead::core

### `VariableQuantity`  *(include/homestead/core/quantity.hpp)*

Represents a three-point estimate for any measurable quantity.

| Field | Type | Constraints |
|---|---|---|
| `min` | `double` | ≥ 0.0; ≤ expected |
| `expected` | `double` | ≥ min; ≤ max |
| `max` | `double` | ≥ expected |

Helper constructors:
- `VariableQuantity(double fixed)` — sets all three to the same value.
- `VariableQuantity(double min, double expected, double max)` — validated on construction;
  returns error via `std::expected` if invariant violated.

### `MonthMask`  *(include/homestead/core/quantity.hpp)*

```cpp
using MonthMask = std::uint16_t;  // bits 0–11 = Jan–Dec
constexpr MonthMask ALL_MONTHS = 0x0FFF;
constexpr MonthMask NO_MONTHS  = 0x0000;
```

Helper: `bool is_active(MonthMask mask, int month)` where month ∈ [0, 11].

### `ResourceCategory`  *(include/homestead/core/resource.hpp)*

```cpp
enum class ResourceCategory {
    raw_material,
    feed,
    fertilizer,
    food_product,
    building_material,
    fuel,
    water,
    labor_hours,
    money,
    waste,
    other
};
```

### `ChemicalComposition`  *(include/homestead/core/resource.hpp)*

```cpp
using ChemicalComposition = std::unordered_map<std::string, double>;
// key: element/compound slug (e.g. "N_percent", "P_percent", "organic_matter_percent")
// value: quantity in the resource's natural unit (% dry weight, mg/kg, etc.)
```

### `NutritionalProfile`  *(include/homestead/core/resource.hpp)*

Only present (`std::optional`) for `ResourceCategory::food_product` and `feed`.

| Field | Type | Unit |
|---|---|---|
| `calories_kcal_per_kg` | `double` | kcal/kg |
| `protein_g_per_kg` | `double` | g/kg |
| `fat_g_per_kg` | `double` | g/kg |
| `carbs_g_per_kg` | `double` | g/kg |
| `micronutrients` | `std::unordered_map<std::string, double>` | mg/kg by nutrient slug |

### `PhysicalProperties`  *(include/homestead/core/resource.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `weight_kg_per_unit` | `double` | > 0 |
| `volume_liters_per_unit` | `double` | ≥ 0 (0 = N/A) |
| `shelf_life_days` | `int` | -1 = indefinite; ≥ 0 otherwise |

### `Resource`  *(include/homestead/core/resource.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `slug` | `std::string` | non-empty; `[a-z0-9_]` only; unique in Registry |
| `name` | `std::string` | non-empty; human-readable |
| `category` | `ResourceCategory` | required |
| `composition` | `ChemicalComposition` | may be empty |
| `nutrition` | `std::optional<NutritionalProfile>` | present iff food/feed |
| `physical` | `PhysicalProperties` | required |

### `Lifecycle`  *(include/homestead/core/entity.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `setup_days` | `int` | ≥ 0 |
| `cycle_days` | `int` | > 0 |
| `cycles_per_year` | `double` | > 0 |
| `active_months` | `MonthMask` | ≥ 1 bit set |

### `ResourceFlowSpec`  *(include/homestead/core/entity.hpp)*

One input or output row on an entity template.

| Field | Type | Constraints |
|---|---|---|
| `resource_slug` | `std::string` | must exist in Registry at entity registration time |
| `quantity_per_cycle` | `VariableQuantity` | all components > 0 for outputs; ≥ 0 for inputs |
| `timing_within_cycle` | `double` | [0.0, 1.0]; 0 = start, 1 = end of cycle |

### `InfrastructureSpec`  *(include/homestead/core/entity.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `area_m2` | `VariableQuantity` | min ≥ 0 |
| `construction_materials` | `std::vector<ResourceFlowSpec>` | may be empty |
| `initial_labor_hours` | `VariableQuantity` | min ≥ 0 |
| `initial_cost` | `VariableQuantity` | min ≥ 0 |

### `Entity`  *(include/homestead/core/entity.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `slug` | `std::string` | non-empty; `[a-z0-9_]`; unique in Registry |
| `name` | `std::string` | non-empty |
| `description` | `std::string` | may be empty |
| `inputs` | `std::vector<ResourceFlowSpec>` | all resource_slugs must exist in Registry |
| `outputs` | `std::vector<ResourceFlowSpec>` | ≥ 1 output; all resource_slugs must exist |
| `lifecycle` | `Lifecycle` | required |
| `operating_labor_per_cycle` | `VariableQuantity` | min ≥ 0 |
| `stocking_density` | `VariableQuantity` | units per m²; min ≥ 0 |
| `infrastructure` | `InfrastructureSpec` | required |

### `RegistryError`  *(include/homestead/core/registry.hpp)*

```cpp
enum class RegistryErrorKind {
    duplicate_slug,
    unknown_resource_slug,  // referenced in entity inputs/outputs
    invalid_quantity,       // VariableQuantity invariant violated
    malformed_slug          // slug contains disallowed characters
};

struct RegistryError {
    RegistryErrorKind kind;
    std::string message;
    std::string offending_slug;  // the slug that caused the error
};
```

### `Registry`  *(include/homestead/core/registry.hpp)*

| Operation | Signature | Notes |
|---|---|---|
| Load defaults | `static Registry load_defaults()` | Loads `default_registry.json`; never fails (embedded) |
| Register resource | `std::expected<void, RegistryError> register_resource(Resource)` | Eager slug validation |
| Register entity | `std::expected<void, RegistryError> register_entity(Entity)` | Validates all resource_slug refs eagerly |
| Find resource | `std::optional<Resource> find_resource(std::string_view slug) const` | Returns nullopt if not found |
| Find entity | `std::optional<Entity> find_entity(std::string_view slug) const` | Returns nullopt if not found |
| All resources | `std::span<const Resource> resources() const` | Stable view |
| All entities | `std::span<const Entity> entities() const` | Stable view |
| Producers of | `std::vector<std::string> producers_of(std::string_view resource_slug) const` | Returns entity slugs whose outputs include this resource |

**Override semantics**: Re-registering with an existing slug silently replaces the
previous entry (custom always wins).

---

## Module: homestead::graph

### `NodeId`  *(include/homestead/graph/node.hpp)*

```cpp
using NodeId = std::uint64_t;
constexpr NodeId INVALID_NODE = std::numeric_limits<NodeId>::max();
```

### `NodeKind`  *(include/homestead/graph/node.hpp)*

```cpp
enum class NodeKind { entity_instance, external_source, goal_sink };
```

### `EntityInstanceNode`  *(include/homestead/graph/node.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `id` | `NodeId` | assigned by Graph |
| `instance_name` | `std::string` | non-empty; unique within graph |
| `entity_slug` | `std::string` | must exist in Registry at graph build time |
| `quantity` | `VariableQuantity` | min > 0 |
| `schedule` | `MonthMask` | ≥ 1 bit set |

### `ExternalSourceNode`  *(include/homestead/graph/node.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `id` | `NodeId` | assigned by Graph |
| `resource_slug` | `std::string` | must exist in Registry |
| `cost_per_unit` | `double` | ≥ 0 |

### `GoalSinkNode`  *(include/homestead/graph/node.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `id` | `NodeId` | assigned by Graph |
| `resource_slug` | `std::string` | must exist in Registry |
| `quantity_per_month` | `VariableQuantity` | min ≥ 0 |

### `ResourceFlow`  *(include/homestead/graph/edge.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `from` | `NodeId` | must exist in Graph |
| `to` | `NodeId` | must exist in Graph; ≠ from |
| `resource_slug` | `std::string` | must exist in Registry |
| `quantity_per_cycle` | `VariableQuantity` | min > 0 |

### `GraphError`  *(include/homestead/graph/graph.hpp)*

```cpp
enum class GraphErrorKind {
    duplicate_node_name,
    unknown_node,
    self_loop,
    unknown_resource_slug
};
struct GraphError { GraphErrorKind kind; std::string message; };
```

### `Graph`  *(include/homestead/graph/graph.hpp)*

Adjacency-list directed graph. Node data stored in `std::unordered_map<NodeId, NodeVariant>`
where `NodeVariant = std::variant<EntityInstanceNode, ExternalSourceNode, GoalSinkNode>`.

| Operation | Signature | Notes |
|---|---|---|
| Add entity instance | `NodeId add_entity_instance(EntityInstanceNode)` | Auto-assigns NodeId |
| Add external source | `NodeId add_external_source(ExternalSourceNode)` | Auto-assigns NodeId |
| Add goal sink | `NodeId add_goal_sink(GoalSinkNode)` | Auto-assigns NodeId |
| Add flow | `std::expected<void, GraphError> add_flow(ResourceFlow)` | |
| Remove node | `std::expected<void, GraphError> remove_node(NodeId)` | Removes attached edges |
| Successors | `std::span<const NodeId> successors(NodeId) const` | Direct downstream neighbours |
| Predecessors | `std::span<const NodeId> predecessors(NodeId) const` | Direct upstream neighbours |
| Find cycles | `std::vector<std::vector<NodeId>> find_cycles() const` | Johnson's algorithm |
| Has cycle | `bool has_cycle() const` | DFS-based; faster for existence only |
| Unsatisfied inputs | `std::vector<std::pair<NodeId, std::string>> unsatisfied_inputs(const Registry&) const` | Returns (node_id, resource_slug) pairs |
| Node count | `std::size_t node_count() const` | |
| Edge count | `std::size_t edge_count() const` | |
| Get node | `std::optional<NodeVariant> get_node(NodeId) const` | |

---

## Module: homestead::solver

### `SolverConfig`  *(include/homestead/solver/config.hpp)*

| Field | Type | Default | Constraints |
|---|---|---|---|
| `scenario` | `Scenario` enum | `expected` | `optimistic` / `expected` / `pessimistic` |
| `max_area_m2` | `std::optional<double>` | nullopt | > 0 if set |
| `max_budget` | `std::optional<double>` | nullopt | > 0 if set |
| `max_labor_hours_per_month` | `std::optional<double>` | nullopt | > 0 if set |
| `max_convergence_iterations` | `int` | 100 | > 0 |

### `ProductionGoal`  *(include/homestead/solver/config.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `resource_slug` | `std::string` | must exist in Registry |
| `quantity_per_month` | `VariableQuantity` | min ≥ 0 |

### `MonthlyValues`  *(include/homestead/solver/result.hpp)*

```cpp
using MonthlyValues = std::array<double, 12>;  // index 0 = January
```

### `ResourceBalance`  *(include/homestead/solver/result.hpp)*

| Field | Type | Notes |
|---|---|---|
| `resource_slug` | `std::string` | |
| `internal_production` | `MonthlyValues` | Produced by plan entity instances |
| `consumption` | `MonthlyValues` | Consumed by plan entity instances + goals |
| `external_purchase` | `MonthlyValues` | From ExternalSourceNodes |
| `annual_internal_production` | `double` | Sum of internal_production |
| `annual_consumption` | `double` | Sum of consumption |
| `annual_external_purchase` | `double` | Sum of external_purchase |

### `DiagnosticKind`  *(include/homestead/solver/result.hpp)*

```cpp
enum class DiagnosticKind {
    unsatisfied_goal,       // goal quantity cannot be met (area/budget/no-producer)
    area_exceeded,          // plan would exceed max_area_m2 constraint
    non_convergent_cycle,   // fixed-point did not converge within iteration limit
    missing_producer,       // no registry entity can produce a required resource
    seasonality_gap         // goal resource unavailable in one or more months
};
```

### `Diagnostic`  *(include/homestead/solver/result.hpp)*

| Field | Type | Notes |
|---|---|---|
| `kind` | `DiagnosticKind` | |
| `message` | `std::string` | Human-readable description |
| `resource_slug` | `std::optional<std::string>` | Affected resource, if applicable |
| `entity_slug` | `std::optional<std::string>` | Affected entity, if applicable |

### `InfrastructureBOM`  *(include/homestead/solver/result.hpp)*

| Field | Type | Notes |
|---|---|---|
| `total_area_m2` | `double` | Sum of all entity instance area requirements |
| `materials` | `std::vector<std::pair<std::string, double>>` | resource_slug → total quantity |
| `estimated_initial_cost` | `double` | Sum of infrastructure + material costs |
| `initial_labor_hours` | `double` | Sum of setup labor across all instances |

### `PlanResult`  *(include/homestead/solver/result.hpp)*

| Field | Type | Notes |
|---|---|---|
| `graph` | `Graph` | Fully resolved graph with all entity instances, sources, sinks |
| `balance_sheet` | `std::vector<ResourceBalance>` | One entry per resource referenced in plan |
| `gap_report` | `std::vector<ResourceBalance>` | Subset where `annual_external_purchase > 0` |
| `labor_schedule` | `MonthlyValues` | Total labor-hours per month across all instances |
| `bom` | `InfrastructureBOM` | |
| `loop_closure_score` | `double` | [0.0, 1.0]; multiply × 100 for % |
| `diagnostics` | `std::vector<Diagnostic>` | Empty = fully satisfied plan |

### `ISolverStrategy`  *(include/homestead/solver/strategy.hpp)*

```cpp
class ISolverStrategy {
public:
    virtual ~ISolverStrategy() = default;
    virtual PlanResult solve(
        std::span<const ProductionGoal> goals,
        const Registry& registry,
        const SolverConfig& config) = 0;
};
```

### `BackpropagationSolver`  *(include/homestead/solver/solver.hpp)*

Implements `ISolverStrategy`. Default solver. See research.md §1 for algorithm.

### Free function convenience API  *(include/homestead/solver/solver.hpp)*

```cpp
PlanResult solve(
    std::span<const ProductionGoal> goals,
    const Registry& registry,
    const SolverConfig& config = {});
```

---

## Module: homestead::serialization

### `SchemaVersion`  *(include/homestead/serialization/schema_version.hpp)*

| Field | Type | Constraints |
|---|---|---|
| `major` | `int` | ≥ 0 |
| `minor` | `int` | ≥ 0 |
| `patch` | `int` | ≥ 0 |

Key methods:
- `std::string to_string() const` → `"1.0.0"`
- `static std::expected<SchemaVersion, std::string> parse(std::string_view)`
- `bool compatible_with(const SchemaVersion& other) const` → true iff `major == other.major`

Current schema version for each document type:
- Registry: `1.0.0`
- Graph: `1.0.0`
- PlanResult: `1.0.0`

### Serialization functions  *(include/homestead/serialization/serialization.hpp)*

All `to_json` functions produce a JSON object with a top-level `"version"` field
(SemVer string) and a `"data"` object.

| Function | Signature |
|---|---|
| Serialize Registry | `nlohmann::json to_json(const Registry&)` |
| Serialize Graph | `nlohmann::json to_json(const Graph&)` |
| Serialize PlanResult | `nlohmann::json to_json(const PlanResult&)` |
| Deserialize Registry | `std::expected<Registry, std::string> registry_from_json(const nlohmann::json&)` |
| Deserialize Graph | `std::expected<Graph, std::string> graph_from_json(const nlohmann::json&)` |
| Deserialize PlanResult | `std::expected<PlanResult, std::string> plan_from_json(const nlohmann::json&)` |

**Error conditions returned by `from_json` functions**:
- Missing `"version"` field
- Incompatible MAJOR version
- Missing required field (identifies field name in error string)
- Wrong JSON type (identifies field name and expected type)

---

## Entity Relationships

```
Registry ──(owns)──► Resource (1..*)
Registry ──(owns)──► Entity (1..*)
Entity ──(references via slug)──► Resource (inputs + outputs)

Graph ──(contains)──► EntityInstanceNode (0..*)
Graph ──(contains)──► ExternalSourceNode (0..*)
Graph ──(contains)──► GoalSinkNode (0..*)
Graph ──(contains)──► ResourceFlow edges (0..*)

EntityInstanceNode ──(instantiates via slug)──► Entity (in Registry)
ResourceFlow ──(references via slug)──► Resource (in Registry)

PlanResult ──(contains)──► Graph
PlanResult ──(contains)──► ResourceBalance (0..*)
PlanResult ──(contains)──► InfrastructureBOM
PlanResult ──(contains)──► Diagnostic (0..*)
```

**Key invariant**: All slug references in Graph nodes and ResourceFlows MUST resolve
in the Registry used during graph construction and solving. The Registry is passed
by const-ref to solver and graph validation functions — it is never mutated after
initial loading.
