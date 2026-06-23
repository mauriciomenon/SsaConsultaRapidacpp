#pragma once

#include <string>
#include <string_view>

namespace ssa::domain {

    [[nodiscard]] inline std::string normalizedDerivationMode(const std::string_view value) {
        if (value == "root" || value == "derived") {
            return std::string{value};
        }
        return "all";
    }

} // namespace ssa::domain
