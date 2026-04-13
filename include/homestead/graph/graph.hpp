#pragma once

#include <homestead/core/registry.hpp>
#include <homestead/graph/edge.hpp>
#include <homestead/graph/node.hpp>

#include <concepts>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace homestead {

/// Categorises graph mutation and query errors.
enum class GraphErrorKind {
    duplicate_node_name,   ///< An EntityInstanceNode with this instance_name already exists
    unknown_node,          ///< NodeId not present in the graph
    self_loop,             ///< Attempted to add an edge from a node to itself
    unknown_resource_slug  ///< ResourceFlow references a resource not in the Registry
};

/// Returned by Graph operations when a precondition is violated.
struct GraphError {
    /// Classification of the failure.
    GraphErrorKind kind;
    /// Human-readable description.
    std::string message;
};

/// Directed resource-flow graph.
///
/// Nodes are entity instances, external sources, or goal sinks.
/// Edges are ResourceFlow values representing inter-node resource transfers.
/// The graph may contain cycles; use has_cycle() / find_cycles() to inspect them.
class Graph {
   public:
    Graph() = default;

    // ── Mutation ───────────────────────────────────────────────────────────────

    /// Adds an entity-instance node. Auto-assigns a unique NodeId.
    NodeId add_entity_instance(EntityInstanceNode node);

    /// Adds an external-source node. Auto-assigns a unique NodeId.
    NodeId add_external_source(ExternalSourceNode node);

    /// Adds a goal-sink node. Auto-assigns a unique NodeId.
    NodeId add_goal_sink(GoalSinkNode node);

    /// Adds a directed resource-flow edge.
    /// Returns GraphError::self_loop if from == to.
    /// Returns GraphError::unknown_node if either node is absent.
    [[nodiscard]] std::expected<void, GraphError> add_flow(ResourceFlow flow);

    /// Removes a node and all its attached edges.
    [[nodiscard]] std::expected<void, GraphError> remove_node(NodeId id);

    // ── Traversal ──────────────────────────────────────────────────────────────

    /// Returns the direct downstream neighbours of a node.
    [[nodiscard]] std::span<const NodeId> successors(NodeId id) const;

    /// Returns the direct upstream neighbours of a node.
    [[nodiscard]] std::span<const NodeId> predecessors(NodeId id) const;

    /// Returns the node variant for the given id, or nullopt if absent.
    [[nodiscard]] std::optional<NodeVariant> get_node(NodeId id) const;

    // ── Cycle detection ────────────────────────────────────────────────────────

    /// Returns true if the graph contains at least one directed cycle.
    /// Uses iterative DFS with white/grey/black colouring — O(V+E).
    [[nodiscard]] bool has_cycle() const;

    /// Enumerates all simple directed cycles using Johnson's algorithm.
    /// Returns a list of node-id sequences; each sequence begins and ends at
    /// the same node (start == end omitted — the list contains distinct nodes).
    [[nodiscard]] std::vector<std::vector<NodeId>> find_cycles() const;

    // ── Gap analysis ───────────────────────────────────────────────────────────

    /// For each EntityInstanceNode, identifies required inputs that have no
    /// incoming ResourceFlow edge.  Returns (node_id, resource_slug) pairs.
    [[nodiscard]] std::vector<std::pair<NodeId, std::string>> unsatisfied_inputs(
        const Registry& registry) const;

    // ── Traversal (typed) ──────────────────────────────────────────────────────

    /// Invokes fn(NodeId, const NodeVariant&) for every valid node in the graph.
    /// Iteration order is unspecified.  Safe after any sequence of remove_node()
    /// calls — unlike linear-scan patterns, no ID gaps are skipped or missed.
    template <std::invocable<NodeId, const NodeVariant&> Callable>
    void for_each_node(Callable&& fn) const {
        for (const auto& [id, node] : nodes_) {
            std::forward<Callable>(fn)(id, node);
        }
    }

    // ── Metrics ────────────────────────────────────────────────────────────────

    /// Total number of nodes.
    [[nodiscard]] std::size_t node_count() const noexcept;

    /// Total number of directed edges.
    [[nodiscard]] std::size_t edge_count() const noexcept;

   private:
    NodeId next_id_{0};

    std::unordered_map<NodeId, NodeVariant> nodes_;
    std::unordered_map<NodeId, std::vector<NodeId>> successors_;
    std::unordered_map<NodeId, std::vector<NodeId>> predecessors_;
    std::vector<ResourceFlow> edges_;

    NodeId assign_id() noexcept { return next_id_++; }
};

}  // namespace homestead
