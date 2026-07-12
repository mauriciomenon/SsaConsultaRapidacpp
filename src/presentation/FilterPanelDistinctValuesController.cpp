#include "presentation/FilterPanelDistinctValuesController.h"

#include "query/SsaQueryService.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ssa::presentation {

    std::uint64_t FilterPanelDistinctValuesController::DistinctValueRequestContext::start() {
        ++token;
        return token;
    }

    bool FilterPanelDistinctValuesController::DistinctValueRequestContext::accepts(
        std::uint64_t requestToken) const {
        return requestToken == token;
    }

    FilterPanelDistinctValuesController::FilterPanelDistinctValuesController(
        std::shared_ptr<query::SsaQueryService> queryService, filterpanel::FilterPanelState& state,
        QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)), state_(state),
          columnValueOptionsFetcher_(queryService_, this),
          quickSectorOptionsFetcher_(queryService_, this) {
        configureConnections();
    }

    void FilterPanelDistinctValuesController::configureConnections() {
        connect(&columnValueOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesReady, this,
                [this](std::uint64_t requestToken, std::vector<std::string> values,
                       const std::size_t maxValueLength) {
                    onColumnValueOptionsReady(requestToken, std::move(values), maxValueLength);
                });
        connect(&columnValueOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesFailed, this,
                &FilterPanelDistinctValuesController::onColumnValueOptionsFailed);
        connect(&quickSectorOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesReady, this,
                [this](std::uint64_t requestToken, std::vector<std::string> values) {
                    onQuickSectorOptionsReady(requestToken, std::move(values));
                });
        connect(&quickSectorOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesFailed, this,
                &FilterPanelDistinctValuesController::onQuickSectorOptionsFailed);
    }

    void FilterPanelDistinctValuesController::invalidateColumnValueRequests() {
        columnValueRequests_.clear();
        columnValueOptionsFetcher_.clearPendingRequests();
    }

    void FilterPanelDistinctValuesController::refreshColumnValueOptionsFor(
        const QString& key, const std::uint64_t stateVersion) {
        if (!queryService_) {
            emit columnValueOptionsReady({}, 0, key, stateVersion);
            return;
        }
        const auto request = requestBuilder_.columnValuesRequestFor(state_, key.toStdString());
        if (!request.has_value()) {
            emit columnValueOptionsReady({}, 0, key, stateVersion);
            return;
        }

        const auto requestToken = ++nextColumnValueRequestToken_;
        columnValueRequests_[requestToken] = ColumnRequestContext{key, stateVersion};
        columnValueOptionsFetcher_.requestValues(*request, requestToken, true);
    }

    void FilterPanelDistinctValuesController::refreshQuickSectorOptions() {
        if (!queryService_) {
            emit quickSectorOptionsReady({});
            return;
        }

        quickSectorOptionsFetcher_.requestValues(requestBuilder_.quickSectorRequest(state_),
                                                 quickSectorRequest_.start(), false);
    }

    void FilterPanelDistinctValuesController::onColumnValueOptionsReady(
        const std::uint64_t requestToken, std::vector<std::string> values,
        const std::size_t maxValueLength) {
        const auto request = columnValueRequests_.find(requestToken);
        if (request == columnValueRequests_.end()) {
            return;
        }
        const auto context = request->second;
        columnValueRequests_.erase(request);
        emit columnValueOptionsReady(std::move(values), maxValueLength, context.key,
                                     context.stateVersion);
    }

    void FilterPanelDistinctValuesController::onColumnValueOptionsFailed(
        const std::uint64_t requestToken, const QString& message) {
        const auto request = columnValueRequests_.find(requestToken);
        if (request == columnValueRequests_.end()) {
            return;
        }
        const auto context = request->second;
        columnValueRequests_.erase(request);
        emit columnValueOptionsFailed(context.key, context.stateVersion, message);
    }

    void FilterPanelDistinctValuesController::onQuickSectorOptionsReady(
        std::uint64_t requestToken, std::vector<std::string> values) {
        if (!quickSectorRequest_.accepts(requestToken)) {
            return;
        }
        emit quickSectorOptionsReady(std::move(values));
    }

    void FilterPanelDistinctValuesController::onQuickSectorOptionsFailed(
        const std::uint64_t requestToken, const QString& message) {
        if (!quickSectorRequest_.accepts(requestToken)) {
            return;
        }
        emit quickSectorOptionsFailed(message);
    }

} // namespace ssa::presentation
