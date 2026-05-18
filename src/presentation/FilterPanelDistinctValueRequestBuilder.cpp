#include "presentation/FilterPanelDistinctValueRequestBuilder.h"

#include "domain/ColumnCatalog.h"

namespace ssa::presentation {

    namespace {

        constexpr int kColumnValueLimit = 300;
        constexpr int kQuickSectorLimit = 500;

    } // namespace

    void FilterPanelDistinctValueRequestBuilder::applyColumnTermsExcept(
        domain::SsaFilterExpression& filter,
        const std::map<std::string, std::string>& columnFilters,
        const std::string& excludedColumnKey) const {
        for (const auto& [key, value] : columnFilters) {
            if (key != excludedColumnKey) {
                filter.columnTerms.emplace(key, parser_.parseTerms(value));
            }
        }
    }

    std::optional<domain::DistinctValuesRequest>
    FilterPanelDistinctValueRequestBuilder::columnValuesRequest(
        const filterpanel::FilterPanelState& state) const {
        const auto normalizedColumn = state.columnKey().trimmed();
        if (normalizedColumn.isEmpty()) {
            return std::nullopt;
        }
        return columnValuesRequestFor(state, normalizedColumn.toStdString());
    }

    std::optional<domain::DistinctValuesRequest>
    FilterPanelDistinctValueRequestBuilder::columnValuesRequestFor(
        const filterpanel::FilterPanelState& state, const std::string& columnKey) const {
        if (domain::ColumnCatalog::find(columnKey) == nullptr) {
            return std::nullopt;
        }
        domain::DistinctValuesRequest request;
        request.columnKey = columnKey;
        request.filter.quickSector = state.quickSector().trimmed().toStdString();
        request.filter.excludeScaSesSte = state.excludeScaSesSte();
        request.filter.advanced = state.advancedFilters();
        applyColumnTermsExcept(request.filter, state.columnFilters(), request.columnKey);
        request.limit = kColumnValueLimit;
        return request;
    }

    domain::DistinctValuesRequest FilterPanelDistinctValueRequestBuilder::quickSectorRequest(
        const filterpanel::FilterPanelState& state) const {
        domain::DistinctValuesRequest request;
        request.columnKey = std::string{domain::ColumnCatalog::executorColumnKey()};
        request.filter.excludeScaSesSte = state.excludeScaSesSte();
        request.filter.advanced = state.advancedFilters();
        request.limit = kQuickSectorLimit;
        return request;
    }

} // namespace ssa::presentation
