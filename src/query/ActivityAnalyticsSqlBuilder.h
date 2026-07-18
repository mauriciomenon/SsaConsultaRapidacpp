#pragma once

#include "domain/ActivityAnalyticsTypes.h"
#include "query/SqlQueryBuilder.h"

#include <string>

namespace ssa::query {

    [[nodiscard]] std::string canonicalIsoWeekSqlExpression(const std::string& column);

    struct ActivityAnalyticsDimensionQueries final {
        SqlQuery divisions;
        SqlQuery sectors;
        SqlQuery people;
    };

    // Series query columns, in order: bucket_key, division, sector, person,
    // registration_cohort, deadline_class, observed_iso_week, source_revision and count.
    class ActivityAnalyticsSqlBuilder final {
      public:
        explicit ActivityAnalyticsSqlBuilder(std::string sourceTable = "ssa_table",
                                             std::string dataset = "SSA");

        [[nodiscard]] SqlQuery buildSeries(const domain::AnalyticsRequest& request) const;
        [[nodiscard]] SqlQuery buildSeries(const domain::AnalyticsRequest& request,
                                           const SqlWhereClause& sourceFilter) const;
        [[nodiscard]] ActivityAnalyticsDimensionQueries
        buildDimensionValues(const domain::AnalyticsRequest& request) const;
        [[nodiscard]] SqlQuery buildEventAvailability() const;
        [[nodiscard]] SqlQuery buildStockAvailability() const;
        [[nodiscard]] SqlQuery buildAvailability() const;
        [[nodiscard]] SqlQuery buildSnapshotMetadata(const domain::AnalyticsRequest& request) const;

      private:
        std::string sourceTable_;
        std::string dataset_;
    };

} // namespace ssa::query
