#pragma once

#include "presentation/BrowseQueryState.h"
#include "presentation/SearchViewModel.h"

#include <QObject>
#include <QString>

namespace ssa::presentation {

    class BrowseInputCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit BrowseInputCoordinator(BrowseQueryState& queryState, SearchViewModel& search,
                                        QObject* parent = nullptr);

        [[nodiscard]] bool setPageSize(const int value);
        void apply();
        void clearSearchAndResetPage();
        [[nodiscard]] bool nextPage();
        [[nodiscard]] bool previousPage();
        [[nodiscard]] bool applySortByColumn(const QString& key);

      private:
        BrowseQueryState& queryState_;
        SearchViewModel& search_;
    };

} // namespace ssa::presentation
