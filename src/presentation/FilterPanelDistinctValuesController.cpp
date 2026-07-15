#include "presentation/FilterPanelDistinctValuesController.h"

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
        std::shared_ptr<ports::ISsaBrowsePort> browsePort, filterpanel::FilterPanelState& state,
        QObject* parent)
        : QObject(parent), browsePort_(std::move(browsePort)), state_(state),
          columnValueOptionsFetcher_(browsePort_, this),
          quickSectorOptionsFetcher_(browsePort_, this) {
        configureConnections();
    }

    void FilterPanelDistinctValuesController::configureConnections() {
        connect(&columnValueOptionsFetcher_, &FilterPanelDistinctValueFetcher::stateChanged, this,
                &FilterPanelDistinctValuesController::stateChanged);
        connect(&quickSectorOptionsFetcher_, &FilterPanelDistinctValueFetcher::stateChanged, this,
                &FilterPanelDistinctValuesController::stateChanged);
        connect(&columnValueOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesReady, this,
                [this](std::uint64_t requestToken, std::vector<std::string> values,
                       const std::size_t maxValueLength) {
                    onColumnValueOptionsReady(requestToken, std::move(values), maxValueLength);
                });
        connect(&columnValueOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesFailed, this,
                &FilterPanelDistinctValuesController::onColumnValueOptionsFailed);
        connect(&columnValueOptionsFetcher_, &FilterPanelDistinctValueFetcher::valuesCanceled, this,
                &FilterPanelDistinctValuesController::onColumnValueOptionsCanceled);
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

    void FilterPanelDistinctValuesController::cancel() {
        columnValueOptionsFetcher_.cancelRequests();
        quickSectorOptionsFetcher_.cancelRequests();
    }

    bool FilterPanelDistinctValuesController::running() const {
        return columnValueOptionsFetcher_.running() || quickSectorOptionsFetcher_.running();
    }

    void FilterPanelDistinctValuesController::refreshColumnValueOptionsFor(
        const QString& key, const std::uint64_t stateVersion) {
        if (!browsePort_) {
            emit this->columnValueOptionsFailed(key, stateVersion,
                                                QStringLiteral("browse service is not configured"));
            return;
        }
        const auto request = requestBuilder_.columnValuesRequestFor(state_, key.toStdString());
        if (!request.has_value()) {
            emit this->columnValueOptionsReady({}, 0, key, stateVersion);
            return;
        }

        const auto requestToken = ++nextColumnValueRequestToken_;
        columnValueRequests_[requestToken] = ColumnRequestContext{key, stateVersion};
        columnValueOptionsFetcher_.requestValues(*request, requestToken, true);
    }

    void FilterPanelDistinctValuesController::refreshQuickSectorOptions() {
        if (!browsePort_) {
            emit this->quickSectorOptionsFailed(QStringLiteral("browse service is not configured"));
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
        emit this->columnValueOptionsReady(std::move(values), maxValueLength, context.key,
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
        emit this->columnValueOptionsFailed(context.key, context.stateVersion, message);
    }

    void FilterPanelDistinctValuesController::onColumnValueOptionsCanceled(
        const std::uint64_t requestToken) {
        const auto request = columnValueRequests_.find(requestToken);
        if (request == columnValueRequests_.end()) {
            return;
        }
        const auto context = request->second;
        columnValueRequests_.erase(request);
        emit this->columnValueOptionsCanceled(context.key, context.stateVersion);
    }

    void FilterPanelDistinctValuesController::onQuickSectorOptionsReady(
        std::uint64_t requestToken, std::vector<std::string> values) {
        if (!quickSectorRequest_.accepts(requestToken)) {
            return;
        }
        emit this->quickSectorOptionsReady(std::move(values));
    }

    void FilterPanelDistinctValuesController::onQuickSectorOptionsFailed(
        const std::uint64_t requestToken, const QString& message) {
        if (!quickSectorRequest_.accepts(requestToken)) {
            return;
        }
        emit this->quickSectorOptionsFailed(message);
    }

} // namespace ssa::presentation
