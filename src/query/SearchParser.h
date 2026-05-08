#pragma once

#include "domain/SsaTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace ssa::query {

    struct SearchExpression {
        std::vector<domain::FilterTerm> requiredTerms;
    };

    class SearchParser final {
      public:
        [[nodiscard]] SearchExpression parse(std::string_view input) const;
        [[nodiscard]] std::vector<domain::FilterTerm> parseTerms(std::string_view input) const;
    };

} // namespace ssa::query
