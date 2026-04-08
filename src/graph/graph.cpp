#include <homestead/graph/graph.hpp>

#include <algorithm>
#include <format>
#include <functional>
#include <ranges>
#include <stack>
#include <unordered_set>

namespace homestead {

// ── Internal helpers ───────────────────────────────────────────────────────────

static std::string kind_label(GraphErrorKind k) {
    switch (k) {
        case GraphErrorKind::duplicate_node_name:
            return "duplicate_node_name";
        case GraphErrorKind::unknown_node:
            return "unknown_node";
        case GraphErrorKind::self_loop:
            return "self_loop";
        case GraphErrorKind::unknown_resource_slug:
            return "unknown_resource_slug";
    }
    return "unknown";
}

// ── Mutation ───────────────────────────────────────────────────────────────────

NodeId Graph::add_entity_instance(EntityInstanceNode node) {
    NodeId id = assign_id();
    node.id = id;
    nodes_.emplace(id, std::move(node));
    successors_[id];  // ensure entry exists
    predecessors_[id];
    return id;
}

NodeId Graph::add_external_source(ExternalSourceNode node) {
    NodeId id = assign_id();
    node.id = id;
    nodes_.emplace(id, std::move(node));
    successors_[id];
    predecessors_[id];
    return id;
}

NodeId Graph::add_goal_sink(GoalSinkNode node) {
    NodeId id = assign_id();
    node.id = id;
    nodes_.emplace(id, std::move(node));
    successors_[id];
    predecessors_[id];
    return id;
}

std::expected<void, GraphError> Graph::add_flow(ResourceFlow flow) {
    if (flow.from == flow.to) {
        return std::unexpected(
            GraphError{GraphErrorKind::self_loop, std::format("Self-loop on node {}", flow.from)});
    }
    if (!nodes_.contains(flow.from)) {
        return std::unexpected(GraphError{GraphErrorKind::unknown_node,
                                          std::format("Source node {} not in graph", flow.from)});
    }
    if (!nodes_.contains(flow.to)) {
        return std::unexpected(
            GraphError{GraphErrorKind::unknown_node,
                       std::format("Destination node {} not in graph", flow.to)});
    }
    successors_[flow.from].push_back(flow.to);
    predecessors_[flow.to].push_back(flow.from);
    edges_.push_back(std::move(flow));
    return {};
}

std::expected<void, GraphError> Graph::remove_node(NodeId id) {
    if (!nodes_.contains(id)) {
        return std::unexpected(
            GraphError{GraphErrorKind::unknown_node, std::format("Node {} not in graph", id)});
    }
    // Remove all edges touching this node.
    std::erase_if(edges_, [id](const ResourceFlow& e) { return e.from == id || e.to == id; });
    // Remove adjacency list entries referencing this node.
    for (auto& [nid, succs] : successors_) {
        std::erase(succs, id);
    }
    for (auto& [nid, preds] : predecessors_) {
        std::erase(preds, id);
    }
    successors_.erase(id);
    predecessors_.erase(id);
    nodes_.erase(id);
    return {};
}

// ── Traversal ──────────────────────────────────────────────────────────────────

std::span<const NodeId> Graph::successors(NodeId id) const {
    auto it = successors_.find(id);
    if (it == successors_.end()) {
        return {};
}
    return it->second;
}

std::span<const NodeId> Graph::predecessors(NodeId id) const {
    auto it = predecessors_.find(id);
    if (it == predecessors_.end()) {
        return {};
}
    return it->second;
}

std::optional<NodeVariant> Graph::get_node(NodeId id) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
        return std::nullopt;
}
    return it->second;
}

// ── Cycle detection ────────────────────────────────────────────────────────────

bool Graph::has_cycle() const {
    // Iterative DFS: white=0, grey=1, black=2
    std::unordered_map<NodeId, int> color;
    color.reserve(nodes_.size());
    for (const auto& [id, _] : nodes_) {
        color[id] = 0;
}

    for (const auto& [start, _] : nodes_) {
        if (color[start] != 0) {
            continue;
}

        // Stack stores (node, iterator-index into successor list)
        std::stack<std::pair<NodeId, std::size_t>> stk;
        stk.emplace(start, 0);
        color[start] = 1;

        while (!stk.empty()) {
            auto& [u, idx] = stk.top();
            const auto& succs = successors_.at(u);
            if (idx < succs.size()) {
                NodeId v = succs[idx++];
                if (color[v] == 1) {
                    return true;  // back edge
}
                if (color[v] == 0) {
                    color[v] = 1;
                    stk.emplace(v, 0);
                }
            } else {
                color[u] = 2;
                stk.pop();
            }
        }
    }
    return false;
}

// Johnson's algorithm — find all simple cycles in a directed graph.
// Reference: Donald B. Johnson, "Finding All the Elementary Circuits of a
// Directed Graph", SIAM J. Comput., 4(1), 77–84, 1975.
std::vector<std::vector<NodeId>> Graph::find_cycles() const {
    const std::size_t n = nodes_.size();
    if (n == 0) {
        return {};
}

    // Map NodeId → dense index for the algorithm.
    std::vector<NodeId> index_to_id;
    index_to_id.reserve(n);
    std::unordered_map<NodeId, std::size_t> id_to_index;
    id_to_index.reserve(n);
    for (const auto& [id, _] : nodes_) {
        id_to_index[id] = index_to_id.size();
        index_to_id.push_back(id);
    }

    // Build adjacency list in dense form.
    std::vector<std::vector<std::size_t>> adj(n);
    for (const auto& e : edges_) {
        adj[id_to_index[e.from]].push_back(id_to_index[e.to]);
    }

    std::vector<std::vector<NodeId>> result;
    std::vector<bool> blocked(n, false);
    std::vector<std::unordered_set<std::size_t>> b_set(n);
    std::vector<std::size_t> stk;

    std::function<bool(std::size_t, std::size_t)> circuit = [&](std::size_t v,
                                                                std::size_t s) -> bool {
        bool found = false;
        stk.push_back(v);
        blocked[v] = true;

        for (std::size_t w : adj[v]) {
            if (w == s) {
                // Found a cycle — record it.
                std::vector<NodeId> cycle;
                cycle.reserve(stk.size());
                for (auto i : stk) {
                    cycle.push_back(index_to_id[i]);
}
                result.push_back(std::move(cycle));
                found = true;
            } else if (!blocked[w]) {
                if (circuit(w, s)) {
                    found = true;
}
            }
        }

        if (found) {
            // Unblock v and recursively unblock everything in b_set[v].
            std::function<void(std::size_t)> unblock = [&](std::size_t u) {
                blocked[u] = false;
                for (std::size_t w : b_set[u]) {
                    b_set[u].erase(w);  // safe: iterating copy needed
                    if (blocked[w]) {
                        unblock(w);
}
                }
                b_set[u].clear();
            };
            unblock(v);
        } else {
            for (std::size_t w : adj[v]) {
                b_set[w].insert(v);
            }
        }

        stk.pop_back();
        return found;
    };

    for (std::size_t s = 0; s < n; ++s) {
        // Reset blocked and b_set for the subgraph starting at s.
        std::fill(blocked.begin(), blocked.end(), false);
        for (auto& bs : b_set) {
            bs.clear();
}

        circuit(s, s);
    }

    return result;
}

// ── Gap analysis ───────────────────────────────────────────────────────────────

std::vector<std::pair<NodeId, std::string>> Graph::unsatisfied_inputs(
    const Registry& registry) const {
    std::vector<std::pair<NodeId, std::string>> gaps;

    for (const auto& [id, variant] : nodes_) {
        // Only EntityInstanceNodes have declared inputs.
        if (!std::holds_alternative<EntityInstanceNode>(variant)) {
            continue;
}
        const auto& inst = std::get<EntityInstanceNode>(variant);

        auto entity_opt = registry.find_entity(inst.entity_slug);
        if (!entity_opt) {
            continue;
}

        for (const auto& input_spec : entity_opt->inputs) {
            // Check whether there is an incoming ResourceFlow for this slug.
            bool satisfied = std::ranges::any_of(edges_, [&](const ResourceFlow& e) {
                return e.to == id && e.resource_slug == input_spec.resource_slug;
            });
            if (!satisfied) {
                gaps.emplace_back(id, input_spec.resource_slug);
            }
        }
    }
    return gaps;
}

// ── Metrics ────────────────────────────────────────────────────────────────────

std::size_t Graph::node_count() const noexcept {
    return nodes_.size();
}

std::size_t Graph::edge_count() const noexcept {
    return edges_.size();
}

}  // namespace homestead
