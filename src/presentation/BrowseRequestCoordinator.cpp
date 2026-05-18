#include "presentation/BrowseRequestCoordinator.h"

namespace ssa::presentation {

    BrowseRequestCoordinator::BrowseRequestCoordinator(
        std::shared_ptr<query::SsaQueryService> queryService, BrowseQueryState& queryState,
        SearchViewModel& search, FilterPanelViewModel& filters, StatusViewModel& status,
        SsaTableModel& tableModel, QObject* parent)
        : QObject(parent), queryState_(queryState), search_(search), filters_(filters),
          tableModel_(tableModel), pageLifecycle_(queryState_, tableModel_, status, this),
          pageQueries_(std::move(queryService), this) {
        connect(&pageQueries_, &PageQueryCoordinator::started, this,
                [this] { pageLifecycle_.markRequestStarted(); });
        connect(&pageQueries_, &PageQueryCoordinator::succeeded, this,
                [this](PageQueryResult result, const domain::SsaPageRequest& request) {
                    pageLifecycle_.markRequestSucceeded(std::move(result), request);
                    emit pageChanged();
                });
        connect(&pageQueries_, &PageQueryCoordinator::canceled, this, [this] {
            pageLifecycle_.markRequestCanceled();
            emit pageChanged();
        });
        connect(&pageQueries_, &PageQueryCoordinator::replaced, this,
                [this] { pageLifecycle_.markRequestReplaced(); });
        connect(&pageQueries_, &PageQueryCoordinator::failed, this, [this](const QString& message) {
            pageLifecycle_.markRequestFailed(message);
            emit pageChanged();
        });
    }

    domain::SsaPageRequest BrowseRequestCoordinator::currentRequest() const {
        return buildRequest();
    }

    void BrowseRequestCoordinator::loadCurrentRequest() {
        runRequest(buildRequest());
    }

    void BrowseRequestCoordinator::cancelCurrentRequest() {
        pageQueries_.cancel();
        pageLifecycle_.markRequestCanceled();
    }

    domain::SsaPageRequest BrowseRequestCoordinator::buildRequest() const {
        return queryState_.buildRequest(search_.text().toStdString(), filters_.columnFilters(),
                                        filters_.quickSector().trimmed().toStdString(),
                                        filters_.excludeScaSesSte(), filters_.advancedFilters());
    }

    void BrowseRequestCoordinator::runRequest(const domain::SsaPageRequest& request) {
        pageQueries_.run(request);
    }

} // namespace ssa::presentation
