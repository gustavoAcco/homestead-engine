#include <homestead/core/registry.hpp>

#include <algorithm>
#include <format>
#include <ranges>

#include "log.hpp"

namespace homestead {

// Declared here; defined in default_registry_data.cpp (also in homestead_core).
namespace detail {
void build_default_registry(Registry& reg);
}

// ── Registry::load_defaults ────────────────────────────────────────────────────

Registry Registry::load_defaults() {
    HOMESTEAD_LOG_INFO("Loading default registry");
    Registry reg;
    detail::build_default_registry(reg);
    HOMESTEAD_LOG_INFO("Default registry loaded: {} resources, {} entities", reg.resources().size(),
                       reg.entities().size());
    return reg;
}

// ── Mutation ───────────────────────────────────────────────────────────────────

std::expected<void, RegistryError> Registry::register_resource(Resource resource) {
    if (!is_valid_slug(resource.slug)) {
        return std::unexpected(RegistryError{
            RegistryErrorKind::malformed_slug,
            std::format("Resource slug '{}' is invalid (must be [a-z0-9_]+, non-empty)",
                        resource.slug),
            resource.slug});
    }
    // Remove existing entry with the same slug (override semantics).
    std::erase_if(resources_, [&](const Resource& r) { return r.slug == resource.slug; });
    HOMESTEAD_LOG_DEBUG("Registered resource '{}'", resource.slug);
    resources_.push_back(std::move(resource));
    return {};
}

std::expected<void, RegistryError> Registry::register_entity(Entity entity) {
    if (!is_valid_slug(entity.slug)) {
        return std::unexpected(
            RegistryError{RegistryErrorKind::malformed_slug,
                          std::format("Entity slug '{}' is invalid", entity.slug), entity.slug});
    }
    // Validate all resource slug references.
    auto validate_slug = [&](const std::string& slug) -> std::expected<void, RegistryError> {
        if (!find_resource(slug)) {
            return std::unexpected(RegistryError{
                RegistryErrorKind::unknown_resource_slug,
                std::format("Entity '{}' references unknown resource slug '{}'", entity.slug, slug),
                slug});
        }
        return {};
    };

    for (const auto& flow : entity.inputs) {
        if (auto err = validate_slug(flow.resource_slug); !err) {
            return err;
}
    }
    for (const auto& flow : entity.outputs) {
        if (auto err = validate_slug(flow.resource_slug); !err) {
            return err;
}
    }
    for (const auto& flow : entity.infrastructure.construction_materials) {
        if (auto err = validate_slug(flow.resource_slug); !err) {
            return err;
}
    }

    // Override semantics.
    std::erase_if(entities_, [&](const Entity& e) { return e.slug == entity.slug; });
    HOMESTEAD_LOG_DEBUG("Registered entity '{}'", entity.slug);
    entities_.push_back(std::move(entity));
    return {};
}

// ── Query ──────────────────────────────────────────────────────────────────────

std::optional<Resource> Registry::find_resource(std::string_view slug) const {
    auto it =
        std::ranges::find_if(resources_, [slug](const Resource& r) { return r.slug == slug; });
    if (it == resources_.end()) {
        return std::nullopt;
}
    return *it;
}

std::optional<Entity> Registry::find_entity(std::string_view slug) const {
    auto it = std::ranges::find_if(entities_, [slug](const Entity& e) { return e.slug == slug; });
    if (it == entities_.end()) {
        return std::nullopt;
}
    return *it;
}

std::span<const Resource> Registry::resources() const noexcept {
    return resources_;
}

std::span<const Entity> Registry::entities() const noexcept {
    return entities_;
}

std::vector<std::string> Registry::producers_of(std::string_view resource_slug) const {
    std::vector<std::string> result;
    for (const auto& entity : entities_) {
        for (const auto& output : entity.outputs) {
            if (output.resource_slug == resource_slug) {
                result.push_back(entity.slug);
                break;
            }
        }
    }
    return result;
}

}  // namespace homestead
