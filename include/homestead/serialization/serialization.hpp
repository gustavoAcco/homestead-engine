#pragma once

#include <homestead/core/registry.hpp>
#include <homestead/graph/graph.hpp>
#include <homestead/serialization/schema_version.hpp>
#include <homestead/solver/result.hpp>

#include <expected>
#include <string>

// Pull in the full nlohmann/json header so callers can use nlohmann::json
// without a separate include.
#include <nlohmann/json.hpp>

namespace homestead {

// ── Serialize ────────────────────────────────────────────────────────────────

/// Serializes a Registry to a JSON object with version envelope.
[[nodiscard]] nlohmann::json to_json(const Registry& registry);

/// Serializes a Graph to a JSON object with version envelope.
[[nodiscard]] nlohmann::json to_json(const Graph& graph);

/// Serializes a PlanResult to a JSON object with version envelope.
[[nodiscard]] nlohmann::json to_json(const PlanResult& plan);

// ── Deserialize ──────────────────────────────────────────────────────────────

/// Deserializes a Registry from a JSON object.
/// Returns an error string if the document is malformed or has a breaking
/// schema version mismatch.
[[nodiscard]] std::expected<Registry, std::string> registry_from_json(const nlohmann::json& j);

/// Deserializes a Graph from a JSON object.
[[nodiscard]] std::expected<Graph, std::string> graph_from_json(const nlohmann::json& j);

/// Deserializes a PlanResult from a JSON object.
/// The complete plan is restored from JSON without re-solving.
[[nodiscard]] std::expected<PlanResult, std::string> plan_from_json(const nlohmann::json& j);

}  // namespace homestead
