#include "presentation/FilterPanelDistinctValueRequestBuilder.h"

#include "domain/ColumnCatalog.h"
#include "query/SearchParser.h"

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
                filters.reprogrammingEquals.reset();
                filters.reprogrammingValues.clear();
                filters.reprogrammingComparison = domain::NumericComparisonMode::Equals;
                filters.onlyReprogrammed = false;
            }
            if (excludedColumnKey == domain::ColumnCatalog::derivationColumnKey()) {
                filters.derivationMode = domain::DerivationFilterMode::All;
            }
            return filters;
        }

        void applyColumnTermsExcept(domain::SsaFilterExpression& filter,
                                    const std::map<std::string, std::string>& columnFilters,
                                    const std::string& excludedColumnKey) {
            query::SearchParser parser;
            for (const auto& [key, value] : columnFilters) {
                if (key != excludedColumnKey) {
                    filter.columnTerms.emplace(key, parser.parseTerms(value));
                }
            }
        }

    } // namespace

    std::optional<domain::DistinctValuesRequest>
    FilterPanelDistinctValueRequestBuilder::columnValuesRequest(
        const filterpanel::FilterPanelState& state) {
        const auto normalizedColumn = state.columnKey().trimmed();
        if (normalizedColumn.isEmpty()) {
            return std::nullopt;
        }
        return columnValuesRequestFor(state, normalizedColumn.toStdString());
    }

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
        applyColumnTermsExcept(request.filter, state.columnFilters(), request.columnKey);
        request.limit = domain::kDefaultDistinctValuesLimit;
        return request;
    }

    domain::DistinctValuesRequest FilterPanelDistinctValueRequestBuilder::quickSectorRequest(
        const filterpanel::FilterPanelState& state) {
        domain::DistinctValuesRequest request;
        request.columnKey = std::string{domain::ColumnCatalog::executorColumnKey()};
        request.filter.excludeScaSesSte = state.excludeScaSesSte();
        request.filter.advanced = advancedFiltersExcept(state.advancedFilters(), request.columnKey);
        applyColumnTermsExcept(request.filter, state.columnFilters(), request.columnKey);
        request.limit = kQuickSectorLimit;
        return request;
    }

} // namespace ssa::presentation
