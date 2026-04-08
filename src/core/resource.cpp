#include <homestead/core/resource.hpp>

#include <algorithm>
#include <cctype>
#include <string_view>

namespace homestead {

bool is_valid_slug(std::string_view slug) noexcept {
    if (slug.empty()) {
        return false;
}
    return std::ranges::all_of(
        slug, [](unsigned char c) { return (std::islower(c) != 0) || (std::isdigit(c) != 0) || c == '_'; });
}

}  // namespace homestead
