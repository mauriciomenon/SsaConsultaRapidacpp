#include "presentation/FilterPanelDistinctValueRequestBuilder.h"

#include "domain/ColumnCatalog.h"

#include <map>
#include <string>

namespace ssa::presentation {

    namespace {

        constexpr int kQuickSectorLimit = 500;

        domain::AdvancedFilterSpec advancedFiltersExcept(domain::AdvancedFilterSpec filters,
                                                         const std::string& excludedColumnKey) {
            filters.textFilters.erase(excludedColumnKey);
            if (filters.weekColumnKey == excludedColumnKey) {
                filters.year.reset();
                filters.week.reset();
            }
            if (excludedColumnKey == domain::ColumnCatalog::issueWeekColumnKey()) {
                filters.issueYear.reset();
                filters.issueWeekStart.reset();
                filters.issueWeekEnd.reset();
            }
            if (excludedColumnKey == domain::ColumnCatalog::executionWeekColumnKey()) {
                filters.executionYear.reset();
                filters.executionWeekStart.reset();
                filters.executionWeekEnd.reset();
            }
            if (domain::ColumnCatalog::isReprogrammingColumn(excludedColumnKey)) {
                filters.reprogrammingValues.clear();
                filters.reprogrammingComparison = domain::NumericComparisonMode::Equals;
                filters.onlyReprogrammed = false;
            }
            if (excludedColumnKey == domain::ColumnCatalog::derivationColumnKey()) {
                filters.derivationMode = domain::DerivationFilterMode::All;
            }
            return filters;
        }

    } // namespace

    std::optional<domain::DistinctValuesRequest>
    FilterPanelDistinctValueRequestBuilder::columnValuesRequestFor(
        const filterpanel::FilterPanelState& state, const std::string& columnKey) {
        if (domain::ColumnCatalog::find(columnKey) == nullptr) {
            return std::nullopt;
        }
        domain::DistinctValuesRequest request;
        request.columnKey = columnKey;
        if (!domain::ColumnCatalog::isQuickSectorFilterColumn(request.columnKey)) {
            request.filter.quickSector = state.quickSector().trimmed().toStdString();
        }
        request.filter.excludeScaSesSte =
            !domain::ColumnCatalog::isStatusExclusionFilterColumn(request.columnKey) &&
            state.excludeScaSesSte();
        request.filter.advanced = advancedFiltersExcept(state.advancedFilters(), request.columnKey);
        for (const auto& [key, value] : state.columnFilters()) {
            if (key != request.columnKey) {
                request.columnFilters.emplace(key, value);
            }
        }
        request.limit = kAdvancedDistinctValuesLimit;
        return request;
    }

    domain::DistinctValuesRequest FilterPanelDistinctValueRequestBuilder::quickSectorRequest(
        const filterpanel::FilterPanelState& state) {
        domain::DistinctValuesRequest request;
        request.columnKey = std::string{domain::ColumnCatalog::executorColumnKey()};
        request.filter.excludeScaSesSte = state.excludeScaSesSte();
        request.filter.advanced = advancedFiltersExcept(state.advancedFilters(), request.columnKey);
        for (const auto& [key, value] : state.columnFilters()) {
            if (key != request.columnKey) {
                request.columnFilters.emplace(key, value);
            }
        }
        request.limit = kQuickSectorLimit;
        return request;
    }

} // namespace ssa::presentation
