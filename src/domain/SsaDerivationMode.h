#pragma once

#include <string>

namespace ssa::domain {

    [[nodiscard]] inline std::string normalizedDerivationMode(std::string value) {
        if (value == "root" || value == "derived") {
            return value;
        }
        return "all";
    }

} // namespace ssa::domain
