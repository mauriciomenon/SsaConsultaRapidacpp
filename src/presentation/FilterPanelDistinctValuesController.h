#pragma once

#include "presentation/FilterPanelDistinctValueFetcher.h"
#include "presentation/FilterPanelDistinctValueRequestBuilder.h"
#include "presentation/FilterPanelState.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ssa::query {
    class SsaQueryService;
}

namespace ssa::presentation {

    class FilterPanelDistinctValuesController final : public QObject {
        Q_OBJECT

      public:
        FilterPanelDistinctValuesController(std::shared_ptr<query::SsaQueryService> queryService,
                                            filterpanel::FilterPanelState& state,
                                            QObject* parent = nullptr);

        void refreshColumnValueOptionsFor(const QString& key, std::uint64_t stateVersion = 0);
        void refreshQuickSectorOptions();
        void invalidateColumnValueRequests();

      signals:
        void columnValueOptionsReady(std::vector<std::string> values, std::size_t maxValueLength,
                                     QString key, std::uint64_t stateVersion);
        void columnValueOptionsFailed(QString key, std::uint64_t stateVersion, QString message);
        void columnValueOptionsCanceled(QString key, std::uint64_t stateVersion);
        void quickSectorOptionsReady(std::vector<std::string> values);
        void quickSectorOptionsFailed(QString message);

      private:
        struct DistinctValueRequestContext {
            std::uint64_t token{0};

            std::uint64_t start();
            [[nodiscard]] bool accepts(std::uint64_t requestToken) const;
        };

        struct ColumnRequestContext final {
            QString key;
            std::uint64_t stateVersion{0};
        };

        void configureConnections();
        void onColumnValueOptionsReady(std::uint64_t requestToken, std::vector<std::string> values,
                                       std::size_t maxValueLength);
        void onColumnValueOptionsFailed(std::uint64_t requestToken, const QString& message);
        void onColumnValueOptionsCanceled(std::uint64_t requestToken);
        void onQuickSectorOptionsReady(std::uint64_t requestToken, std::vector<std::string> values);
        void onQuickSectorOptionsFailed(std::uint64_t requestToken, const QString& message);

        std::shared_ptr<query::SsaQueryService> queryService_;
        filterpanel::FilterPanelState& state_;
        FilterPanelDistinctValueRequestBuilder requestBuilder_;
        // Keep independent watchers so sector refreshes do not cancel column value refreshes.
        FilterPanelDistinctValueFetcher columnValueOptionsFetcher_;
        FilterPanelDistinctValueFetcher quickSectorOptionsFetcher_;
        DistinctValueRequestContext quickSectorRequest_;
        std::map<std::uint64_t, ColumnRequestContext> columnValueRequests_;
        std::uint64_t nextColumnValueRequestToken_{0};
    };

} // namespace ssa::presentation
