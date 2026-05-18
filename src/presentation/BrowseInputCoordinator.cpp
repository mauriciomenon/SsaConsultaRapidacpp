#include "presentation/BrowseInputCoordinator.h"

namespace ssa::presentation {

    BrowseInputCoordinator::BrowseInputCoordinator(BrowseQueryState& queryState,
                                                   SearchViewModel& search, QObject* parent)
        : QObject(parent), queryState_(queryState), search_(search) {}

    bool BrowseInputCoordinator::setPageSize(const int value) {
        const auto previousPageSize = queryState_.pageSize();
        queryState_.setPageSize(value);
        return queryState_.pageSize() != previousPageSize;
    }

    void BrowseInputCoordinator::apply() {
        queryState_.resetPage();
    }

    void BrowseInputCoordinator::clearSearchTextAndReload() {
        search_.setText({});
        queryState_.resetPage();
    }

    bool BrowseInputCoordinator::nextPage() {
        return queryState_.nextPage();
    }

    bool BrowseInputCoordinator::previousPage() {
        return queryState_.previousPage();
    }

    bool BrowseInputCoordinator::applySortByColumn(const QString& key) {
        if (key.isEmpty()) {
            return false;
        }
        queryState_.sortByColumnKey(key.toStdString());
        return true;
    }

} // namespace ssa::presentation
