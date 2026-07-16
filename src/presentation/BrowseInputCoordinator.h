#pragma once

#include "presentation/BrowseQueryState.h"

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class BrowseInputCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit BrowseInputCoordinator(BrowseQueryState& queryState, QObject* parent = nullptr);

        [[nodiscard]] bool setPageSize(const int value);
        void apply();
        [[nodiscard]] bool nextPage();
        [[nodiscard]] bool previousPage();
        [[nodiscard]] bool applySortByColumn(const QString& key);
        [[nodiscard]] bool resetSort();

      private:
        BrowseQueryState& queryState_;
    };

} // namespace ssa::presentation
