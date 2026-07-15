#pragma once

#include "domain/SsaTypes.h"
#include "query/SearchParser.h"
#include "query/SqlPredicateBuilder.h"

#include <string>
#include <string_view>
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
        explicit SqlQueryBuilder(std::string tableName = "ssa_table");

        [[nodiscard]] SqlPageQueries build(const domain::SsaPageRequest& request) const;
        [[nodiscard]] SqlPageQueries build(const domain::SsaPageRequest& request,
                                           bool useDerivedCountSummary) const;
        [[nodiscard]] SqlQuery buildRows(const domain::SsaPageRequest& request) const;
        [[nodiscard]] SqlQuery buildRows(const domain::SsaPageRequest& request,
                                         bool useDerivedCountSummary) const;
        [[nodiscard]] SqlQuery buildCount(const domain::SsaPageRequest& request) const;
        [[nodiscard]] SqlQuery buildExecutadasReport(const domain::SsaPageRequest& request,
                                                     bool byDivision) const;
        [[nodiscard]] SqlRecordQuery buildRecordBySsaNumber(const domain::SsaNumber& number) const;
        [[nodiscard]] SqlQuery
        buildDistinctValues(const domain::DistinctValuesRequest& request) const;
        [[nodiscard]] SqlQuery buildMaxValueLength(std::string_view columnKey) const;
        [[nodiscard]] std::string tableName() const;
        [[nodiscard]] const std::string& rawTableName() const noexcept;
        [[nodiscard]] std::string derivedCountSummaryTableName() const;

      private:
        SearchParser parser_;
        SqlPredicateBuilder predicateBuilder_;
        std::string tableName_;
    };

} // namespace ssa::query
