#pragma once

#include "domain/ActivityAnalyticsTypes.h"

#include <stop_token>
#include <vector>

namespace ssa::ports {

    class IActivityAnalyticsPort {
      public:
        virtual ~IActivityAnalyticsPort() = default;

        [[nodiscard]] virtual domain::AnalyticsSeriesResult
        series(const domain::AnalyticsRequest& request, std::stop_token stopToken = {}) const = 0;

        [[nodiscard]] virtual domain::AnalyticsDimensionValues
        dimensionValues(const domain::AnalyticsRequest& request,
                        std::stop_token stopToken = {}) const = 0;

        [[nodiscard]] virtual std::vector<domain::AnalyticsMetricAvailability>
        availability(std::stop_token stopToken = {}) const = 0;
    };

} // namespace ssa::ports
