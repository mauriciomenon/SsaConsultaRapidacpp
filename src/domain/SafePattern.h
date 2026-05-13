#pragma once

#include "domain/SsaTypes.h"

#include <string_view>

namespace ssa::domain {

    inline constexpr std::string_view kSafePatternWildcard{"."};
    inline constexpr std::string_view kUnsupportedSafePatternOperators{"(){}|*+?"};

    [[nodiscard]] inline bool safePatternUnsupported(const std::string_view pattern) {
        return pattern.size() > kMaxSafePatternLength ||
               pattern.find_first_of(kUnsupportedSafePatternOperators) != std::string_view::npos;
    }

} // namespace ssa::domain
