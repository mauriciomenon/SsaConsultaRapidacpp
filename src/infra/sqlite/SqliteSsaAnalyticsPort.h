#pragma once

#include "ports/IActivityAnalyticsPort.h"
#include "ports/IActivityAnalyticsSettingsPort.h"
#include "query/ActivityAnalyticsSqlBuilder.h"

#include <filesystem>

namespace ssa::infra::sqlite {

    class SqliteSsaAnalyticsPort final : public ports::IActivityAnalyticsPort,
                                         public ports::IActivityAnalyticsSettingsPort {
      public:
        explicit SqliteSsaAnalyticsPort(std::filesystem::path dbPath);

        [[nodiscard]] domain::AnalyticsSeriesResult
        series(const domain::AnalyticsRequest& request,
               std::stop_token stopToken = {}) const override;

        [[nodiscard]] domain::AnalyticsDimensionValues
        dimensionValues(const domain::AnalyticsRequest& request,
                        std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::vector<domain::AnalyticsMetricAvailability>
        availability(std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::optional<int>
        warningWindowDays(std::stop_token stopToken = {}) const override;

        void setWarningWindowDays(int days, std::stop_token stopToken = {}) override;

      private:
        std::filesystem::path dbPath_;
        query::ActivityAnalyticsSqlBuilder queryBuilder_;
    };

} // namespace ssa::infra::sqlite
