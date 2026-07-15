#include "presentation/BrowseRequestCoordinator.h"
#include "presentation/FilterPanelSectorViewModel.h"

#include <QObject>

namespace ssa::presentation {

    BrowseRequestCoordinator::BrowseRequestCoordinator(
        std::shared_ptr<ports::ISsaBrowsePort> browsePort, BrowseQueryState& queryState,
        SearchViewModel& search, FilterPanelViewModel& filters, StatusViewModel& status,
        SsaTableModel& tableModel, QObject* parent)
        : QObject(parent), queryState_(queryState), search_(search), filters_(filters),
          tableModel_(tableModel), pageLifecycle_(queryState_, tableModel_, status, this),
          pageQueries_(std::move(browsePort), this) {
        connect(&pageQueries_, &PageQueryCoordinator::activeOperationsChanged, this,
                &BrowseRequestCoordinator::activeOperationsChanged);
        connect(&pageQueries_, &PageQueryCoordinator::started, this,
                [this] { pageLifecycle_.markRequestStarted(); });
        connect(&pageQueries_, &PageQueryCoordinator::stateChanged, this,
                [this](const PageQueryCoordinator::State state) {
                    if (state == PageQueryCoordinator::State::Canceling) {
                        pageLifecycle_.markRequestCanceling();
                    }
                });
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
    }

    void BrowseRequestCoordinator::invalidateTotalRowsAll() {
        pageQueries_.invalidateTotalRowsAll();
    }

    bool BrowseRequestCoordinator::hasActiveOperations() const {
        return pageQueries_.hasActiveOperations();
    }

    domain::SsaPageRequest BrowseRequestCoordinator::buildRequest() const {
        auto* sector = qobject_cast<FilterPanelSectorViewModel*>(filters_.sector());
        return queryState_.buildRequest(search_.text().toStdString(), filters_.columnFilters(),
                                        filters_.quickSector().trimmed().toStdString(),
                                        sector->excludeScaSesSte(), filters_.advancedFilters());
    }

    void BrowseRequestCoordinator::runRequest(const domain::SsaPageRequest& request) {
        pageQueries_.run(request);
    }

} // namespace ssa::presentation
