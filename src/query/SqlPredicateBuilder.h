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
        explicit SqlPredicateBuilder(std::string tableName);

        [[nodiscard]] SqlWhereClause build(const SearchExpression& expression,
                                           const domain::SsaFilterExpression& filter) const;

      private:
        std::string tableName_;
    };

} // namespace ssa::query
