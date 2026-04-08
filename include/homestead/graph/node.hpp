#pragma once

#include <homestead/core/quantity.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace homestead {

/// Opaque node identifier. Assigned monotonically by Graph.
using NodeId = std::uint64_t;

/// Sentinel value for an uninitialised or invalid NodeId.
inline constexpr NodeId INVALID_NODE = std::numeric_limits<NodeId>::max();

/// Classification of a graph node.
enum class NodeKind {
    entity_instance,  ///< A running instance of a registry Entity
    external_source,  ///< An external supplier of a resource (purchased)
    goal_sink         ///< A production goal consuming a resource
};

/// A concrete instantiation of a Registry Entity in the plan graph.
struct EntityInstanceNode {
    /// Graph-assigned identifier.
    NodeId id{INVALID_NODE};
    /// User-chosen name for this instance; unique within the graph.
    std::string instance_name;
    /// Slug of the Entity template in the Registry.
    std::string entity_slug;
    /// Number of units (e.g., 50 birds, 3 tanks). min > 0.
    VariableQuantity quantity;
    /// Months in which this instance is active.
    MonthMask schedule{ALL_MONTHS};
};

/// An external supply node; resources here are purchased rather than produced.
struct ExternalSourceNode {
    /// Graph-assigned identifier.
    NodeId id{INVALID_NODE};
    /// Resource being supplied externally.
    std::string resource_slug;
    /// Cost per unit in the plan's currency. ≥ 0.
    double cost_per_unit{};
};

/// Represents a desired production goal in the graph.
struct GoalSinkNode {
    /// Graph-assigned identifier.
    NodeId id{INVALID_NODE};
    /// Resource being demanded.
    std::string resource_slug;
    /// Required quantity per month. min ≥ 0.
    VariableQuantity quantity_per_month;
};

/// Variant holding any graph node type.
using NodeVariant = std::variant<EntityInstanceNode, ExternalSourceNode, GoalSinkNode>;

/// Returns the NodeKind for a NodeVariant.
[[nodiscard]] inline NodeKind node_kind(const NodeVariant& v) noexcept {
    return std::visit(
        [](const auto& n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, EntityInstanceNode>) {
                return NodeKind::entity_instance;
            }
            if constexpr (std::is_same_v<T, ExternalSourceNode>) {
                return NodeKind::external_source;
            }
            return NodeKind::goal_sink;
        },
        v);
}

/// Returns the NodeId stored in any NodeVariant.
[[nodiscard]] inline NodeId node_id(const NodeVariant& v) noexcept {
    return std::visit([](const auto& n) { return n.id; }, v);
}

}  // namespace homestead
