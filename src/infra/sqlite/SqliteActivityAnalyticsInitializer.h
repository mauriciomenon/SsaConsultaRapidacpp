#pragma once

#include "infra/sqlite/SqliteActivityAnalyticsProjection.h"

#include <filesystem>
#include <stop_token>
#include <string>

namespace ssa::infra::sqlite {

    class SqliteActivityAnalyticsInitializer final {
      public:
        [[nodiscard]] static ActivityAnalyticsCaptureResult
        initialize(const std::filesystem::path& dbPath, int observedIsoYearWeek,
                   std::string observedDate, const std::stop_token& stopToken = {});
    };

} // namespace ssa::infra::sqlite
