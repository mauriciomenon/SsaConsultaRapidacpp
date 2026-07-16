#pragma once

#include "domain/SsaTypes.h"
#include "ports/IUserPreferencesStore.h"

#include <QString>

#include <map>
#include <string>
#include <vector>

namespace ssa::presentation {

    class BrowseQueryState final {
      public:
        [[nodiscard]] int pageNumber() const;
        [[nodiscard]] int pageCount() const;
        [[nodiscard]] qlonglong totalRows() const;
        [[nodiscard]] qlonglong totalRowsAll() const;
        [[nodiscard]] int pageSize() const;
        [[nodiscard]] QString sortColumnKey() const;
        [[nodiscard]] bool sortAscending() const;
        [[nodiscard]] const std::vector<std::string>& visibleColumns() const;
        [[nodiscard]] const std::map<std::string, int>& columnWidths() const;

        void setPageSize(int value);
        void resetPage();
        bool nextPage();
        bool previousPage();
        void applyPageResult(const domain::SsaPageResult& result,
                             const std::size_t totalRowsAllValue);
        void sortByColumnKey(const std::string& columnKey);
        void resetSort();
        void applyColumnSettings(std::vector<std::string> visibleColumnsValue,
                                 std::map<std::string, int> columnWidthsValue);
        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;

        [[nodiscard]] domain::SsaPageRequest
        buildRequest(std::string searchText, std::map<std::string, std::string> columnFilters,
                     std::string quickSector, bool excludeScaSesSte,
                     domain::AdvancedFilterSpec advancedFilters) const;

      private:
        std::vector<std::string> visibleColumns_;
        std::map<std::string, int> columnWidths_;
        domain::SortSpec sort_;
        std::size_t pageIndex_{0};
        std::size_t pageSize_{domain::kDefaultPageSize};
        std::size_t totalRows_{0};
        std::size_t totalRowsAll_{0};
    };

} // namespace ssa::presentation
