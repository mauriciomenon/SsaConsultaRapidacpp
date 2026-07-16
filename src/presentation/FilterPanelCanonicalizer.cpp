#include "presentation/FilterPanelCanonicalizer.h"

#include "domain/ColumnCatalog.h"
#include "domain/TextFilterToken.h"
#include "presentation/FilterPanelStateHelpers.h"

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

        const bool includesExcluded =
            domain::ColumnCatalog::containsExcludedStatusCode(
                state.advanced().textFilter(statusColumnKey()).toStdString()) ||
            [&state] {
                const auto columnFilter = state.columnFilters().find(
                    std::string{domain::ColumnCatalog::statusColumnKey()});
                return columnFilter != state.columnFilters().end() &&
                       domain::ColumnCatalog::containsExcludedStatusCode(columnFilter->second);
            }();
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
            domain::parseTextFilterTokens(state.advanced().textFilter(executorKey).toStdString());
        if (tokens.ordered.size() == 1 &&
            tokens.ordered.front().filterOperator == domain::TextFilterOperator::Equals) {
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
        domain::TextFilterTokenSet tokens;
        domain::addTextFilterValue(tokens, value.toStdString(), domain::TextFilterOperator::Equals);
        changed = state.advanced().setTextFilter(
                      executorKey, QString::fromStdString(domain::joinTextFilterTokens(tokens))) ||
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
