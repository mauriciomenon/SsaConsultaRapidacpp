#include "presentation/BrowseSelectionCoordinator.h"

namespace ssa::presentation {

    BrowseSelectionCoordinator::BrowseSelectionCoordinator(DetailsViewModel& details,
                                                           SsaTableModel& tableModel,
                                                           QObject* parent)
        : QObject(parent), details_(details), tableModel_(tableModel) {}

    void BrowseSelectionCoordinator::selectRow(const int row) const {
        const auto record = tableModel_.recordAt(row);
        if (!record) {
            details_.clearRecord();
            return;
        }
        details_.setRecord(*record);
    }

    void BrowseSelectionCoordinator::clearSelection() {
        details_.clearRecord();
    }

} // namespace ssa::presentation
