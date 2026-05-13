#pragma once

#include "domain/SsaTypes.h"
#include "query/SearchParser.h"

#include <string>
#include <vector>

namespace ssa::query {

    struct SqlWhereClause {
        std::string sql;
        std::vector<std::string> bindings;
    };

    class SqlPredicateBuilder final {
      public:
        [[nodiscard]] SqlWhereClause build(const SearchExpression& expression,
                                           const domain::SsaFilterExpression& filter) const;
    };

} // namespace ssa::query
