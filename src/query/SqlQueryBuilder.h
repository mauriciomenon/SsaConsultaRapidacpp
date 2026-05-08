#pragma once

#include "domain/SsaTypes.h"
#include "query/SearchParser.h"

#include <string>
#include <vector>

namespace ssa::query {

    struct SqlQuery {
        std::string sql;
        std::vector<std::string> bindings;
    };

    struct SqlPageQueries {
        SqlQuery page;
        SqlQuery count;
    };

    struct SqlRecordQuery {
        SqlQuery record;
    };

    class SqlQueryBuilder final {
      public:
        [[nodiscard]] SqlPageQueries build(const domain::SsaPageRequest& request) const;
        [[nodiscard]] SqlRecordQuery buildRecordById(const domain::SsaId& id) const;
        [[nodiscard]] SqlQuery
        buildDistinctValues(const domain::DistinctValuesRequest& request) const;

      private:
        SearchParser parser_;
    };

} // namespace ssa::query
