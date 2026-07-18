#pragma once

#include "domain/ActivityAnalyticsTypes.h"
#include "ports/IActivityAnalyticsPort.h"
#include "ports/IActivityAnalyticsSettingsPort.h"

#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace ssa::application {

    struct ActivityAnalyticsDashboard final {
        domain::AnalyticsSeriesResult registeredBySector;
        domain::AnalyticsSeriesResult registeredMonthly;
        domain::AnalyticsSeriesResult executedBySector;
        domain::AnalyticsSeriesResult executedMonthly;
        domain::AnalyticsSeriesResult partialAttentionBySector;
        domain::AnalyticsSeriesResult spgBySector;
        domain::AnalyticsSeriesResult apgBySector;
        domain::AnalyticsSeriesResult aplBySector;
        domain::AnalyticsSeriesResult pendingBySector;
        domain::AnalyticsSeriesResult pendingMonthly;
        domain::AnalyticsSeriesResult issuedByDivision;
        domain::AnalyticsSeriesResult issuedMonthly;
        domain::AnalyticsSeriesResult pendingDeadlineWeekly;

        [[nodiscard]] const domain::AnalyticsSeriesResult&
        pendingDeadlinePercentage() const noexcept;
        [[nodiscard]] const domain::AnalyticsSeriesResult& pendingDeadlineQuantity() const noexcept;
    };

    class ActivityAnalyticsService final {
      public:
        explicit ActivityAnalyticsService(
            std::shared_ptr<const ports::IActivityAnalyticsPort> port,
            std::shared_ptr<ports::IActivityAnalyticsSettingsPort> settingsPort = nullptr);

        [[nodiscard]] domain::AnalyticsSeriesResult
        series(const domain::AnalyticsRequest& request,
               const std::stop_token& stopToken = {}) const;
        [[nodiscard]] domain::AnalyticsDimensionValues
        dimensionValues(const domain::AnalyticsRequest& request,
                        const std::stop_token& stopToken = {}) const;
        [[nodiscard]] std::vector<domain::AnalyticsMetricAvailability>
        availability(const std::stop_token& stopToken = {}) const;
        [[nodiscard]] std::optional<int>
        warningWindowDays(const std::stop_token& stopToken = {}) const;
        void setWarningWindowDays(int days, const std::stop_token& stopToken = {}) const;
        [[nodiscard]] ActivityAnalyticsDashboard
        loadDashboard(const domain::AnalyticsPeriod& reportPeriod,
                      const domain::AnalyticsPeriod& historyPeriod,
                      std::optional<int> warningWindowDays,
                      const std::stop_token& stopToken = {}) const;

      private:
        std::shared_ptr<const ports::IActivityAnalyticsPort> port_;
        std::shared_ptr<ports::IActivityAnalyticsSettingsPort> settingsPort_;
    };

} // namespace ssa::application
