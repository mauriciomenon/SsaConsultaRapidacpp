#pragma once

#include <sqlite3.h>

#include <atomic>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace ssa::infra::sqlite {

    struct ActivityAnalyticsCaptureContext final {
        int observedIsoYearWeek{0};
        std::string observedDate;
        std::string sourceRevision;
        std::string sourceFingerprint;
    };

    struct ActivityAnalyticsCaptureResult final {
        bool changed{false};
        int snapshotMetrics{0};
    };

    class SqliteActivityAnalyticsProjection final {
      public:
        [[nodiscard]] static ActivityAnalyticsCaptureResult
        capture(sqlite3* db, std::string_view sourceTable,
                const ActivityAnalyticsCaptureContext& context,
                const std::stop_token& stopToken = {},
                const std::atomic_bool* busyCancellationObserved = nullptr);

        [[nodiscard]] static std::string
        sourceFingerprint(std::vector<std::string> verifiedSources);

        [[nodiscard]] static std::string
        canonicalSourceFingerprint(sqlite3* db, std::string_view sourceTable,
                                   const std::stop_token& stopToken = {},
                                   const std::atomic_bool* busyCancellationObserved = nullptr);
    };

} // namespace ssa::infra::sqlite
