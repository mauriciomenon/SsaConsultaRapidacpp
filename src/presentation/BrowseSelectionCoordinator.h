#pragma once

#include "presentation/DetailsViewModel.h"
#include "presentation/SsaTableModel.h"

#include <QObject>

namespace ssa::presentation {

    class BrowseSelectionCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit BrowseSelectionCoordinator(DetailsViewModel& details, SsaTableModel& tableModel,
                                            QObject* parent = nullptr);

        void selectRow(int row);
        void clearSelection();
        [[nodiscard]] int currentRow() const;
        void setPendingFirstRow();
        void setPendingLastRow();
        [[nodiscard]] int pendingRow() const;
        void consumePendingRow();

      signals:
        void currentRowChanged(int row);

      private:
        DetailsViewModel& details_;
        SsaTableModel& tableModel_;
        mutable int currentRow_{-1};
        int pendingRow_{-1};
    };

} // namespace ssa::presentation
