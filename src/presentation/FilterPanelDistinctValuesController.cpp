#include "presentation/FilterPanelDistinctValuesController.h"

#include "query/SsaQueryService.h"

#include <utility>

namespace ssa::presentation {

    std::uint64_t
    FilterPanelDistinctValuesController::DistinctValueRequestContext::start(QString requestKey) {
        ++token;
        key = std::move(requestKey);
        return token;
    }

    bool FilterPanelDistinctValuesController::DistinctValueRequestContext::accepts(
        const std::uint64_t requestToken) const {
        return requestToken == token;
    }

    FilterPanelDistinctValuesController::FilterPanelDistinctValuesController(
        std::shared_ptr<query::SsaQueryService> queryService, filterpanel::FilterPanelState& state,
        QObject* parent)
        : QObject(parent), queryService_(std::move(queryService)), state_(state),
          columnValueOptionsFetcher_(queryService_, this),
          quickSectorOptionsFetcher_(queryService_, this), columnValueRefreshTimer_(this) {
        configureConnections();
    }

    void FilterPanelDistinctValuesController::configureConnections() {
        columnValueRefreshTimer_.setInterval(250);
        columnValueRefreshTimer_.setSingleShot(true);
        connect(&columnValueRefreshTimer_, &QTimer::timeout, this,
                [this]() { refreshColumnValueOptions(); });
        connect(&columnValueOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesReady, this,
                [this](const std::uint64_t requestToken, std::vector<std::string> values) {
                    onColumnValueOptionsReady(requestToken, std::move(values));
                });
        connect(&quickSectorOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesReady, this,
                [this](const std::uint64_t requestToken, std::vector<std::string> values) {
                    onQuickSectorOptionsReady(requestToken, std::move(values));
                });
    }

    void FilterPanelDistinctValuesController::scheduleColumnValueRefresh() {
        columnValueRefreshTimer_.start();
    }

    void FilterPanelDistinctValuesController::refreshColumnValueOptions() {
        requestColumnValueOptionsFor(state_.columnKey());
    }

    void FilterPanelDistinctValuesController::refreshColumnValueOptionsFor(const QString& key) {
        requestColumnValueOptionsFor(key);
    }

    void FilterPanelDistinctValuesController::requestColumnValueOptionsFor(QString key) {
        if (!queryService_) {
            emit columnValueOptionsReady({}, key);
            return;
        }
        const auto request = requestBuilder_.columnValuesRequestFor(state_, key.toStdString());
        if (!request.has_value()) {
            emit columnValueOptionsReady({}, key);
            return;
        }

        const auto requestToken = columnValueRequest_.start(key);
        columnValueOptionsFetcher_.requestValues(*request, requestToken);
    }

    void FilterPanelDistinctValuesController::refreshQuickSectorOptions() {
        if (!queryService_) {
            emit quickSectorOptionsReady({});
            return;
        }

        quickSectorOptionsFetcher_.requestValues(requestBuilder_.quickSectorRequest(state_),
                                                 quickSectorRequest_.start());
    }

    void FilterPanelDistinctValuesController::onColumnValueOptionsReady(
        const std::uint64_t requestToken, std::vector<std::string> values) {
        if (!columnValueRequest_.accepts(requestToken)) {
            return;
        }
        emit columnValueOptionsReady(std::move(values), columnValueRequest_.key);
    }

    void FilterPanelDistinctValuesController::onQuickSectorOptionsReady(
        const std::uint64_t requestToken, std::vector<std::string> values) {
        if (!quickSectorRequest_.accepts(requestToken)) {
            return;
        }
        emit quickSectorOptionsReady(std::move(values));
    }

} // namespace ssa::presentation
