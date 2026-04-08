#pragma once

#include <homestead/core/entity.hpp>
#include <homestead/core/resource.hpp>

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace homestead {

/// Categorises the type of registry validation failure.
enum class RegistryErrorKind {
    duplicate_slug,         ///< A resource or entity with this slug already exists
    unknown_resource_slug,  ///< Entity references a resource slug not in the registry
    invalid_quantity,       ///< A VariableQuantity invariant was violated
    malformed_slug          ///< Slug contains disallowed characters or is empty
};

/// Returned by Registry registration operations when validation fails.
struct RegistryError {
    /// Classification of the error.
    RegistryErrorKind kind;
    /// Human-readable description including context.
    std::string message;
    /// The slug that caused the error.
    std::string offending_slug;
};

/// The Registry holds all Resource and Entity templates.
/// It is the single source of truth for slug resolution throughout the engine.
class Registry {
   public:
    /// Constructs an empty registry.
    Registry() = default;

    /// Loads the built-in default registry (tropical agriculture).
    /// Never fails — defaults are embedded at compile time.
    [[nodiscard]] static Registry load_defaults();

    // ── Mutation ───────────────────────────────────────────────────────────────

    /// Registers a resource. Re-registering an existing slug replaces it.
    /// Returns error if slug is malformed.
    [[nodiscard]] std::expected<void, RegistryError> register_resource(Resource resource);

    /// Registers an entity. All resource_slug references in inputs/outputs
    /// must already exist in the registry. Re-registering replaces.
    [[nodiscard]] std::expected<void, RegistryError> register_entity(Entity entity);

    // ── Query ──────────────────────────────────────────────────────────────────

    /// Returns the resource with the given slug, or nullopt if not found.
    [[nodiscard]] std::optional<Resource> find_resource(std::string_view slug) const;

    /// Returns the entity with the given slug, or nullopt if not found.
    [[nodiscard]] std::optional<Entity> find_entity(std::string_view slug) const;

    /// Returns a span over all registered resources (stable for the lifetime
    /// of the registry without further modification).
    [[nodiscard]] std::span<const Resource> resources() const noexcept;

    /// Returns a span over all registered entities.
    [[nodiscard]] std::span<const Entity> entities() const noexcept;

    /// Returns slugs of all entities that produce the given resource.
    [[nodiscard]] std::vector<std::string> producers_of(std::string_view resource_slug) const;

   private:
    std::vector<Resource> resources_;
    std::vector<Entity> entities_;
};

}  // namespace homestead
