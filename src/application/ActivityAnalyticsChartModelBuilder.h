#pragma once

#include "domain/ActivityAnalyticsTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ssa::application {

    struct ActivityAnalyticsDashboard;

    enum class AnalyticsChartMode : std::uint8_t {
        SimpleBar,
        CohortStacked,
        DeadlineStacked,
        LineTotal,
        Custom,
    };

    enum class AnalyticsChartScale : std::uint8_t {
        Quantity,
        Percentage,
    };

    struct AnalyticsChartSeries final {
        std::string name;
        std::vector<std::optional<double>> values;
        std::vector<std::optional<double>> trendValues;
        std::optional<double> total;

        bool operator==(const AnalyticsChartSeries&) const = default;
    };

    struct AnalyticsChartModel final {
        std::vector<std::string> categories;
        std::vector<AnalyticsChartSeries> series;
        std::optional<double> total;
        std::string subtitle;
        std::string qualityNote;
        bool percentage{false};

        bool operator==(const AnalyticsChartModel&) const = default;
    };

    struct ActivityAnalyticsDashboardCharts final {
        AnalyticsChartModel registeredBySector;
        AnalyticsChartModel registeredMonthly;
        AnalyticsChartModel executedBySector;
        AnalyticsChartModel executedMonthly;
        AnalyticsChartModel partialAttentionBySector;
        AnalyticsChartModel spgBySector;
        AnalyticsChartModel apgBySector;
        AnalyticsChartModel aplBySector;
        AnalyticsChartModel pendingBySector;
        AnalyticsChartModel pendingMonthly;
        AnalyticsChartModel issuedByDivision;
        AnalyticsChartModel issuedMonthly;
        AnalyticsChartModel pendingDeadlinePercentage;
        AnalyticsChartModel pendingDeadlineQuantity;
    };

    class ActivityAnalyticsChartModelBuilder final {
      public:
        [[nodiscard]] static AnalyticsChartModel
        build(const domain::AnalyticsRequest& request, const domain::AnalyticsSeriesResult& result,
              AnalyticsChartMode mode, AnalyticsChartScale scale = AnalyticsChartScale::Quantity);

        [[nodiscard]] static ActivityAnalyticsDashboardCharts
        buildDashboard(const domain::AnalyticsPeriod& reportPeriod,
                       const domain::AnalyticsPeriod& historyPeriod,
                       const ActivityAnalyticsDashboard& dashboard);
    };

    // A stock bucket is zero only when its observation metadata exists. A bucket without an
    // observation remains a gap, so missing historical captures are never invented.

} // namespace ssa::application
