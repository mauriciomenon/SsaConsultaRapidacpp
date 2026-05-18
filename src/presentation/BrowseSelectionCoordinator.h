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

        void selectRow(int row) const;
        void clearSelection();

      private:
        DetailsViewModel& details_;
        SsaTableModel& tableModel_;
    };

} // namespace ssa::presentation
