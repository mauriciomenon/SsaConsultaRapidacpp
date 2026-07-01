#pragma once

#include "presentation/FilterPanelDistinctValueFetcher.h"
#include "presentation/FilterPanelDistinctValueRequestBuilder.h"
#include "presentation/FilterPanelState.h"

#include <QObject>
#include <QString>
#include <QTimer>

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

        void scheduleColumnValueRefresh();
        void refreshColumnValueOptions();
        Q_INVOKABLE void refreshColumnValueOptionsFor(const QString& key);
        void preloadColumnValueOptionsFor(const QStringList& keys, std::uint64_t stateVersion);
        void refreshQuickSectorOptions();
        void invalidateColumnValueRequests();

      signals:
        void columnValueOptionsReady(std::vector<std::string> values, QString key,
                                     std::uint64_t stateVersion);
        void quickSectorOptionsReady(std::vector<std::string> values);

      private:
        struct DistinctValueRequestContext {
            std::uint64_t token{0};
            QString key;

            std::uint64_t start(QString requestKey = {});
            void invalidate();
            [[nodiscard]] bool accepts(std::uint64_t requestToken) const;
        };

        struct ColumnRequestContext final {
            QString key;
            std::uint64_t stateVersion{0};
        };

        void configureConnections();
        void requestColumnValueOptionsFor(const QString& key, std::uint64_t stateVersion);
        void onColumnValueOptionsReady(std::uint64_t requestToken, std::vector<std::string> values);
        void onQuickSectorOptionsReady(std::uint64_t requestToken, std::vector<std::string> values);

        std::shared_ptr<query::SsaQueryService> queryService_;
        filterpanel::FilterPanelState& state_;
        FilterPanelDistinctValueRequestBuilder requestBuilder_;
        // Keep independent watchers so sector refreshes do not cancel column value refreshes.
        FilterPanelDistinctValueFetcher columnValueOptionsFetcher_;
        FilterPanelDistinctValueFetcher quickSectorOptionsFetcher_;
        QTimer columnValueRefreshTimer_;
        DistinctValueRequestContext quickSectorRequest_;
        std::map<std::uint64_t, ColumnRequestContext> columnValueRequests_;
        std::uint64_t nextColumnValueRequestToken_{0};
    };

} // namespace ssa::presentation
