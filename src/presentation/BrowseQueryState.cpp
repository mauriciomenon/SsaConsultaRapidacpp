#include "presentation/BrowseQueryState.h"

#include "domain/ColumnCatalog.h"

#include <QString>

#include <algorithm>
#include <utility>

namespace ssa::presentation {

    int BrowseQueryState::pageNumber() const {
        if (totalRows_ == 0) {
            return 0;
        }
        return static_cast<int>(pageIndex_ + 1);
    }

    int BrowseQueryState::pageCount() const {
        return static_cast<int>(domain::pageCount(totalRows_, pageSize_));
    }

    qlonglong BrowseQueryState::totalRows() const {
        return static_cast<qlonglong>(totalRows_);
    }

    int BrowseQueryState::pageSize() const {
        return static_cast<int>(pageSize_);
    }

    QString BrowseQueryState::sortColumnKey() const {
        return QString::fromStdString(sort_.columnKey);
    }

    bool BrowseQueryState::sortAscending() const {
        return sort_.ascending;
    }

    const std::vector<std::string>& BrowseQueryState::visibleColumns() const {
        return visibleColumns_;
    }

    const std::map<std::string, int>& BrowseQueryState::columnWidths() const {
        return columnWidths_;
    }

    void BrowseQueryState::setPageSize(const int value) {
        const int bounded = std::max(domain::clampPageSize(value), domain::kMinPageSize);
        const auto boundedSize = static_cast<std::size_t>(bounded);
        if (pageSize_ == boundedSize) {
            return;
        }
        if (totalRows_ == 0) {
            pageSize_ = boundedSize;
            pageIndex_ = 0;
            return;
        }
        pageSize_ = boundedSize;
        pageIndex_ = 0;
    }

    void BrowseQueryState::resetPage() {
        pageIndex_ = 0;
    }

    bool BrowseQueryState::nextPage() {
        const auto pages = static_cast<std::size_t>(pageCount());
        if (pages == 0 || pageIndex_ + 1 >= pages) {
            return false;
        }
        ++pageIndex_;
        return true;
    }

    bool BrowseQueryState::previousPage() {
        if (pageIndex_ == 0) {
            return false;
        }
        --pageIndex_;
        return true;
    }

    void BrowseQueryState::applyPageResult(const domain::SsaPageResult& result) {
        totalRows_ = result.totalRows;
        pageIndex_ = result.pageIndex;
        if (totalRows_ == 0) {
            pageIndex_ = 0;
        }
    }

    void BrowseQueryState::sortByColumnKey(const std::string& columnKey) {
        if (sort_.columnKey == columnKey) {
            sort_.ascending = !sort_.ascending;
        } else {
            sort_.columnKey = columnKey;
            sort_.ascending = true;
        }
        sort_.statusLast = domain::requiresStatusLastSort(columnKey);
        resetPage();
    }

    void BrowseQueryState::applyColumnSettings(std::vector<std::string> visibleColumns,
                                               std::map<std::string, int> columnWidths) {
        if (visibleColumns_ != visibleColumns) {
            resetPage();
        }
        visibleColumns_ = std::move(visibleColumns);
        columnWidths_ = std::move(columnWidths);
    }

    void BrowseQueryState::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        pageSize_ = static_cast<std::size_t>(domain::clampPageSize(snapshot.pageSize));
        visibleColumns_ = snapshot.visibleColumns.empty()
                              ? domain::ColumnCatalog::defaultVisibleKeys()
                              : snapshot.visibleColumns;
        if (domain::ColumnCatalog::contains(snapshot.sortColumnKey)) {
            sort_.columnKey = snapshot.sortColumnKey;
            sort_.ascending = snapshot.sortAscending;
            sort_.statusLast = domain::requiresStatusLastSort(sort_.columnKey);
        }
        columnWidths_ = snapshot.columnWidths;
    }

    void BrowseQueryState::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        snapshot.visibleColumns = visibleColumns_;
        snapshot.columnWidths = columnWidths_;
        snapshot.pageSize = static_cast<int>(pageSize_);
        snapshot.sortColumnKey = sort_.columnKey;
        snapshot.sortAscending = sort_.ascending;
    }

    domain::SsaPageRequest
    BrowseQueryState::buildRequest(std::string searchText,
                                   std::map<std::string, std::string> columnFilters,
                                   std::string quickSector, const bool excludeScaSesSte,
                                   domain::AdvancedFilterSpec advancedFilters) const {
        domain::SsaPageRequest request;
        request.pageIndex = pageIndex_;
        request.pageSize = pageSize_;
        request.searchText = std::move(searchText);
        request.columnFilters = std::move(columnFilters);
        request.quickSector = std::move(quickSector);
        request.excludeScaSesSte = excludeScaSesSte;
        request.advancedFilters = std::move(advancedFilters);
        request.sort = sort_;
        request.visibleColumns = visibleColumns_;
        return request;
    }

} // namespace ssa::presentation
