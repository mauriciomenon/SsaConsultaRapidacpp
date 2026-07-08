#include "presentation/FilterPanelCanonicalizer.h"

#include "domain/ColumnCatalog.h"
#include "presentation/FilterPanelStateHelpers.h"
#include "query/TextFilterToken.h"

#include <algorithm>
#include <string>

namespace ssa::presentation::filterpanel {

    QString executorColumnKey() {
        return QString::fromStdString(std::string{domain::ColumnCatalog::executorColumnKey()});
    }

    QString statusColumnKey() {
        return QString::fromStdString(std::string{domain::ColumnCatalog::statusColumnKey()});
    }

    bool removeColumnFiltersShadowedByAdvancedText(FilterPanelState& state) {
        bool changed = false;
        for (const auto& [key, value] : state.advanced().textFilters()) {
            changed = state.removeColumnFilter(QString::fromStdString(key)) || changed;
        }
        return changed;
    }

    bool clearStatusExclusionIfStatusIncludesExcluded(FilterPanelState& state) {
        if (!state.excludeScaSesSte()) {
            return false;
        }

        query::TextFilterTokenSet tokens = query::parseTextFilterTokens(
            state.advanced().textFilter(statusColumnKey()).toStdString());
        if (const auto columnFilter = state.columnFilters().find(statusColumnKey().toStdString());
            columnFilter != state.columnFilters().end()) {
            const auto columnTokens = query::parseTextFilterTokens(columnFilter->second);
            for (const auto& token : columnTokens.ordered) {
                query::addTextFilterValue(tokens, token.value, token.filterOperator);
            }
        }

        const auto excluded = domain::ColumnCatalog::excludedStatusCodes();
        const bool includesExcluded =
            std::ranges::any_of(tokens.ordered, [excluded](const auto& token) {
                return token.filterOperator == query::TextFilterOperator::Equals &&
                       std::ranges::find(excluded, std::string_view{token.value}) != excluded.end();
            });
        return includesExcluded && state.setExcludeScaSesSte(false);
    }

    bool foldQuickSectorIntoAdvancedExecutor(FilterPanelState& state) {
        const auto executorKey = executorColumnKey();
        const auto executorExpression = state.advanced().textFilter(executorKey);
        if (state.quickSector().trimmed().isEmpty() || executorExpression.trimmed().isEmpty()) {
            return false;
        }
        const auto mergedExpression = QString::fromStdString(executorFilterWithQuickSector(
            executorExpression.toStdString(), state.quickSector().toStdString()));
        state.advanced().setTextFilter(executorKey, mergedExpression);
        state.setQuickSector({});
        return true;
    }

    bool clearExecutorShortcut(FilterPanelState& state) {
        const auto executorKey = executorColumnKey();
        bool changed = state.setQuickSector({});
        const auto tokens =
            query::parseTextFilterTokens(state.advanced().textFilter(executorKey).toStdString());
        if (tokens.ordered.size() == 1 &&
            tokens.ordered.front().filterOperator == query::TextFilterOperator::Equals) {
            changed = state.advanced().removeTextFilter(executorKey) || changed;
        }
        changed = state.removeColumnFilter(executorKey) || changed;
        return changed;
    }

    bool setExecutorShortcut(FilterPanelState& state, QString value) {
        value = value.trimmed().toUpper();
        const auto executorKey = executorColumnKey();
        bool changed = state.setQuickSector({});
        if (value.isEmpty()) {
            return clearExecutorShortcut(state) || changed;
        }
        query::TextFilterTokenSet tokens;
        query::addTextFilterValue(tokens, value.toStdString(), query::TextFilterOperator::Equals);
        changed = state.advanced().setTextFilter(
                      executorKey, QString::fromStdString(query::joinTextFilterTokens(tokens))) ||
                  changed;
        changed = state.removeColumnFilter(executorKey) || changed;
        return changed;
    }

    bool foldLegacyQuickSector(FilterPanelState& state) {
        if (state.quickSector().trimmed().isEmpty()) {
            return false;
        }
        if (state.advanced().textFilter(executorColumnKey()).trimmed().isEmpty()) {
            return setExecutorShortcut(state, state.quickSector());
        }
        return foldQuickSectorIntoAdvancedExecutor(state);
    }

} // namespace ssa::presentation::filterpanel
