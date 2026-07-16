#include "presentation/BrowseInputCoordinator.h"

namespace ssa::presentation {

    BrowseInputCoordinator::BrowseInputCoordinator(BrowseQueryState& queryState, QObject* parent)
        : QObject(parent), queryState_(queryState) {}

    bool BrowseInputCoordinator::setPageSize(const int value) {
        const auto previousPageSize = queryState_.pageSize();
        queryState_.setPageSize(value);
        const bool changed = queryState_.pageSize() != previousPageSize;
        if (changed) {
            queryState_.resetPage();
        }
        return changed;
    }

    void BrowseInputCoordinator::apply() {
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

    bool BrowseInputCoordinator::resetSort() {
        if (queryState_.sortColumnKey().isEmpty()) {
            return false;
        }
        queryState_.resetSort();
        return true;
    }

} // namespace ssa::presentation
