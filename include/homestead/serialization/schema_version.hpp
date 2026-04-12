#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace homestead {

/// Semantic-version triple for JSON schema documents.
/// Breaking changes require a MAJOR bump; MINOR/PATCH are informational.
struct SchemaVersion {
    int major{1};
    int minor{0};
    int patch{0};

    /// Returns "major.minor.patch" string.
    [[nodiscard]] std::string to_string() const;

    /// Parses "major.minor.patch"; returns error string on failure.
    [[nodiscard]] static std::expected<SchemaVersion, std::string> parse(std::string_view sv);

    /// Two versions are compatible when their MAJOR numbers are equal.
    [[nodiscard]] bool compatible_with(const SchemaVersion& other) const noexcept {
        return major == other.major;
    }

    bool operator==(const SchemaVersion&) const = default;

    /// Lexicographic ordering: major, then minor, then patch.
    [[nodiscard]] bool operator<(const SchemaVersion& o) const noexcept {
        if (major != o.major) {
            return major < o.major;
        }
        if (minor != o.minor) {
            return minor < o.minor;
        }
        return patch < o.patch;
    }
};

/// Current schema versions for each document type.
inline constexpr SchemaVersion REGISTRY_SCHEMA_VERSION{1, 0, 0};
inline constexpr SchemaVersion GRAPH_SCHEMA_VERSION{1, 0, 0};
inline constexpr SchemaVersion PLAN_SCHEMA_VERSION{1, 1, 0};

}  // namespace homestead
