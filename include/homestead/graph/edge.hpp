#pragma once

#include <homestead/core/quantity.hpp>
#include <homestead/graph/node.hpp>

#include <string>

namespace homestead {

/// A directed resource-flow edge connecting two graph nodes.
/// Represents the transfer of a specific resource from a producer to a consumer.
struct ResourceFlow {
    /// Source node — the producer.
    NodeId from{INVALID_NODE};
    /// Destination node — the consumer. Must differ from `from`.
    NodeId to{INVALID_NODE};
    /// Resource being transferred.
    std::string resource_slug;
    /// Quantity transferred per production cycle. min > 0.
    VariableQuantity quantity_per_cycle;
};

}  // namespace homestead
