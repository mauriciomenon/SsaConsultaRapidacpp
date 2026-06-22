#pragma once

#include "presentation/DetailsViewModel.h"
#include "presentation/SsaTableModel.h"

#include <QObject>

namespace ssa::presentation {

    class BrowseSelectionCoordinator final : public QObject {
        Q_OBJECT

      public:
        static constexpr int kNoSelection = -1;
        static constexpr int kNoPendingRow = -1;
        static constexpr int kPendingLastRow = -2;
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
        int currentRow_{kNoSelection};
        int pendingRow_{kNoPendingRow};
    };

    inline constexpr int BrowseSelectionCoordinator::kNoSelection;
    inline constexpr int BrowseSelectionCoordinator::kPendingLastRow;
} // namespace ssa::presentation
