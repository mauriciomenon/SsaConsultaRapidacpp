#pragma once

#include "domain/SsaTypes.h"
#include "query/SsaQueryService.h"

#include <QFutureWatcher>
#include <QObject>

#include <cstdint>
#include <memory>
#include <vector>

namespace ssa::presentation {

    class FilterPanelDistinctValueFetcher final : public QObject {
        Q_OBJECT

      public:
        explicit FilterPanelDistinctValueFetcher(
            std::shared_ptr<query::SsaQueryService> queryService, QObject* parent = nullptr);

        void requestValues(const domain::DistinctValuesRequest& request,
                           const std::uint64_t requestToken);

      signals:
        void valuesReady(std::uint64_t requestToken, std::vector<std::string> values);

      private:
        void onWatcherFinished();
        std::shared_ptr<query::SsaQueryService> queryService_;
        std::uint64_t activeRequestToken_{0};
        QFutureWatcher<std::vector<std::string>> watcher_;
    };

} // namespace ssa::presentation
