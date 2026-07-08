#include "presentation/FilterPanelState.h"

#include "domain/ColumnCatalog.h"
#include "presentation/FilterPanelStateHelpers.h"
#include "query/TextFilterToken.h"

#include <QStringList>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace ssa::presentation::filterpanel {

    FilterPanelState::FilterPanelState(const std::string_view defaultFilterColumnKey)
        : quickSector_{},
          columnKey_(QString::fromUtf8(defaultFilterColumnKey.data(),
                                       static_cast<qsizetype>(defaultFilterColumnKey.size()))) {}

    const QString& FilterPanelState::quickSector() const {
        return quickSector_;
    }

    bool FilterPanelState::setQuickSector(QString value) {
        value = value.trimmed();
        if (quickSector_ == value) {
            return false;
        }
        quickSector_ = std::move(value);
        return true;
    }

    bool FilterPanelState::excludeScaSesSte() const {
        return excludeScaSesSte_;
    }

    bool FilterPanelState::setExcludeScaSesSte(const bool value) {
        if (excludeScaSesSte_ == value) {
            return false;
        }
        excludeScaSesSte_ = value;
        return true;
    }

    const QString& FilterPanelState::columnKey() const {
        return columnKey_;
    }

    bool FilterPanelState::setColumnKey(const QString& value) {
        const auto normalized = value.trimmed();
        if (columnKey_ == normalized) {
            return false;
        }
        columnKey_ = normalized;
        return true;
    }

    const QString& FilterPanelState::columnValue() const {
        return columnValue_;
    }

    bool FilterPanelState::setColumnValue(QString value) {
        if (columnValue_ == value) {
            return false;
        }
        columnValue_ = std::move(value);
        return true;
    }

    FilterPanelAdvancedState& FilterPanelState::advanced() {
        return advanced_;
    }

    const FilterPanelAdvancedState& FilterPanelState::advanced() const {
        return advanced_;
    }

    const std::map<std::string, std::string>& FilterPanelState::columnFilters() const {
        return columnFilters_;
    }

    bool FilterPanelState::setColumnFilters(std::map<std::string, std::string> filters) {
        bool advancedChanged = false;
        for (const auto& [key, value] : filters) {
            advancedChanged =
                advanced_.removeTextFilter(QString::fromStdString(key)) || advancedChanged;
        }
        if (columnFilters_ == filters) {
            return advancedChanged;
        }
        columnFilters_ = std::move(filters);
        return true;
    }

    bool FilterPanelState::addColumnFilter(const QString& key, const QString& value) {
        const auto normalizedKey = key.trimmed();
        const auto normalizedValue = value.trimmed();
        if (normalizedKey.isEmpty() || normalizedValue.isEmpty()) {
            return false;
        }
        const auto keyStd = normalizedKey.toStdString();
        const auto valueStd = normalizedValue.toStdString();

        const bool advancedChanged = advanced_.removeTextFilter(normalizedKey);
        auto filterEntry = columnFilters_.find(keyStd);
        if (filterEntry == columnFilters_.end()) {
            const auto [insertedIt, inserted] = columnFilters_.emplace(keyStd, valueStd);
            return inserted || advancedChanged;
        }

        if (filterEntry->second.find_first_not_of(" \t\r\n") == std::string::npos) {
            filterEntry->second = valueStd;
            return true;
        }

        if (!filterpanel::hasFilterValue(filterEntry->second, valueStd)) {
            filterEntry->second += ", " + valueStd;
            return true;
        }
        return advancedChanged;
    }

    bool FilterPanelState::removeColumnFilter(const QString& key) {
        const auto normalizedKey = key.trimmed().toStdString();
        if (normalizedKey.empty()) {
            return false;
        }
        return columnFilters_.erase(normalizedKey) > 0;
    }

    bool FilterPanelState::removeColumnFiltersShadowedByAdvancedText() {
        bool changed = false;
        for (const auto& [key, value] : advanced_.textFilters()) {
            changed = columnFilters_.erase(key) > 0 || changed;
        }
        return changed;
    }

    bool FilterPanelState::clearStatusExclusionIfStatusIncludesExcluded() {
        if (!excludeScaSesSte_) {
            return false;
        }

        const auto statusKey = std::string{domain::ColumnCatalog::statusColumnKey()};
        query::TextFilterTokenSet tokens = query::parseTextFilterTokens(
            advanced_.textFilter(QString::fromStdString(statusKey)).toStdString());
        if (const auto columnFilter = columnFilters_.find(statusKey);
            columnFilter != columnFilters_.end()) {
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
        if (!includesExcluded) {
            return false;
        }
        excludeScaSesSte_ = false;
        return true;
    }

    bool FilterPanelState::foldQuickSectorIntoAdvancedExecutor() {
        const auto executorKey =
            QString::fromStdString(std::string{domain::ColumnCatalog::executorColumnKey()});
        const auto executorExpression = advanced_.textFilter(executorKey);
        if (quickSector_.trimmed().isEmpty() || executorExpression.trimmed().isEmpty()) {
            return false;
        }
        const auto mergedExpression =
            QString::fromStdString(filterpanel::executorFilterWithQuickSector(
                executorExpression.toStdString(), quickSector_.toStdString()));
        advanced_.setTextFilter(executorKey, mergedExpression);
        quickSector_.clear();
        return true;
    }

    domain::AdvancedFilterSpec FilterPanelState::advancedFilters() const {
        return advanced_.filters();
    }

    bool FilterPanelState::hasFilterForColumn(const QString& key) const {
        const auto normalizedKey = key.trimmed().toStdString();
        if (domain::ColumnCatalog::isQuickSectorFilterColumn(normalizedKey) &&
            !quickSector_.trimmed().isEmpty()) {
            return true;
        }
        if (domain::ColumnCatalog::isStatusExclusionFilterColumn(normalizedKey) &&
            excludeScaSesSte_) {
            return true;
        }
        return advanced_.hasFilterForColumn(key) || columnFilters_.contains(normalizedKey);
    }

    bool FilterPanelState::applyPreferences(const ports::UserPreferencesSnapshot& snapshot,
                                            const QStringList& weekColumnKeys) {
        const QString nextQuickSector =
            QString::fromStdString(snapshot.filters.quickSector).trimmed();
        const bool nextExcludeScaSesSte = snapshot.filters.excludeScaSesSte;
        const auto nextColumnFilters = snapshot.filters.columnFilters;
        const bool advancedChanged = advanced_.applyPreferences(snapshot, weekColumnKeys);

        const bool changed = quickSector_ != nextQuickSector ||
                             excludeScaSesSte_ != nextExcludeScaSesSte ||
                             columnFilters_ != nextColumnFilters || advancedChanged;
        if (!changed) {
            return false;
        }

        quickSector_ = nextQuickSector;
        excludeScaSesSte_ = nextExcludeScaSesSte;
        columnFilters_ = nextColumnFilters;
        foldQuickSectorIntoAdvancedExecutor();
        removeColumnFiltersShadowedByAdvancedText();
        clearStatusExclusionIfStatusIncludesExcluded();
        return true;
    }

    void FilterPanelState::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        snapshot.filters.quickSector = quickSector_.trimmed().toStdString();
        snapshot.filters.excludeScaSesSte = excludeScaSesSte_;
        snapshot.filters.columnFilters = columnFilters_;
        advanced_.writePreferences(snapshot);
    }

    void FilterPanelState::clear() {
        quickSector_.clear();
        columnKey_ = QString::fromStdString(domain::ColumnCatalog::defaultFilterColumnKey());
        columnValue_.clear();
        advanced_.clear();
        columnFilters_.clear();
        excludeScaSesSte_ = domain::kDefaultExcludeScaSesSte;
    }

} // namespace ssa::presentation::filterpanel
