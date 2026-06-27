#include "presentation/FilterPanelDistinctValueFetcher.h"

#include <QtConcurrent>

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

namespace {
    std::string trimCopy(std::string value) {
        auto isSpace = [](const unsigned char c) { return std::isspace(c); };
        const auto left = std::find_if_not(value.begin(), value.end(), isSpace);
        if (left == value.end()) {
            return {};
        }
        const auto right = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
        return std::string(left, right);
    }
} // namespace

namespace ssa::presentation {

    FilterPanelDistinctValueFetcher::FilterPanelDistinctValueFetcher(
        std::shared_ptr<query::SsaQueryService> queryService, QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)) {
        connect(&watcher_, &QFutureWatcher<std::vector<std::string>>::finished, this,
                [this] { onWatcherFinished(); });
    }

    void
    FilterPanelDistinctValueFetcher::requestValues(const domain::DistinctValuesRequest& request,
                                                   const std::uint64_t requestToken) {
        if (!queryService_) {
            emit this->valuesReady(requestToken, {});
            return;
        }
        if (watcher_.isRunning()) {
            watcher_.cancel();
        }

        const auto service = queryService_;
        auto requestCopy = request;
        const auto future = QtConcurrent::run([service, requestCopy = std::move(requestCopy)]() {
            auto values = service->distinctValues(requestCopy);
            std::vector<std::string> normalized;
            normalized.reserve(values.size());
            for (auto& value : values) {
                auto trimmed = trimCopy(std::move(value));
                if (!trimmed.empty()) {
                    normalized.push_back(std::move(trimmed));
                }
            }
            return normalized;
        });
        activeRequestToken_ = requestToken;
        watcher_.setFuture(future);
    }

    void FilterPanelDistinctValueFetcher::onWatcherFinished() {
        if (!watcher_.isFinished()) {
            return;
        }
        emit this->valuesReady(activeRequestToken_, watcher_.result());
    }

} // namespace ssa::presentation
