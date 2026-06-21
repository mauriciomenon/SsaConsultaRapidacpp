#include "presentation/BrowseSelectionCoordinator.h"

namespace ssa::presentation {

    BrowseSelectionCoordinator::BrowseSelectionCoordinator(DetailsViewModel& details,
                                                           SsaTableModel& tableModel,
                                                           QObject* parent)
        : QObject(parent), details_(details), tableModel_(tableModel) {}

    void BrowseSelectionCoordinator::selectRow(const int row) {
        const auto record = tableModel_.recordAt(row);
        if (!record) {
            details_.clearRecord();
            return;
        }
        details_.setRecord(*record);
        if (currentRow_ != row) {
            currentRow_ = row;
            emit currentRowChanged(row);
        }
    }

    void BrowseSelectionCoordinator::clearSelection() {
        details_.clearRecord();
        if (currentRow_ != -1) {
            currentRow_ = -1;
            emit currentRowChanged(-1);
        }
    }

    int BrowseSelectionCoordinator::currentRow() const {
        return currentRow_;
    }

} // namespace ssa::presentation
