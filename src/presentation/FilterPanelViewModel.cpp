#include "presentation/FilterPanelViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <utility>
#include <vector>

namespace ssa::presentation {

    FilterPanelViewModel::FilterPanelViewModel(QObject* parent) : QObject(parent) {
        for (const auto& key : domain::ColumnCatalog::filterColumnKeys()) {
            filterColumnKeys_.push_back(QString::fromStdString(key));
        }
        columnKey_ = QString::fromStdString(domain::ColumnCatalog::defaultFilterColumnKey());
        markActiveFiltersDirty();
    }

    QString FilterPanelViewModel::quickSector() const {
        ensureActiveFiltersUpToDate();
        return quickSector_;
    }

    void FilterPanelViewModel::setQuickSector(const QString& value) {
        if (quickSector_ == value) {
            return;
        }
        quickSector_ = value;
        markActiveFiltersDirty();
        emit changed();
    }

    bool FilterPanelViewModel::excludeScaSesSte() const {
        return excludeScaSesSte_;
    }

    void FilterPanelViewModel::setExcludeScaSesSte(const bool value) {
        if (excludeScaSesSte_ == value) {
            return;
        }
        excludeScaSesSte_ = value;
        markActiveFiltersDirty();
        emit changed();
    }

    QStringList FilterPanelViewModel::filterColumnKeys() const {
        return filterColumnKeys_;
    }

    QString FilterPanelViewModel::columnKey() const {
        return columnKey_;
    }

    void FilterPanelViewModel::setColumnKey(const QString& value) {
        if (columnKey_ == value) {
            return;
        }
        columnKey_ = value;
        emit changed();
    }

    QString FilterPanelViewModel::columnValue() const {
        return columnValue_;
    }

    void FilterPanelViewModel::setColumnValue(const QString& value) {
        if (columnValue_ == value) {
            return;
        }
        columnValue_ = value;
        emit changed();
    }

    QStringList FilterPanelViewModel::activeFilters() const {
        ensureActiveFiltersUpToDate();
        return activeFilters_;
    }

    QString FilterPanelViewModel::activeFilterSummary() const {
        ensureActiveFiltersUpToDate();
        return activeFilterSummary_;
    }

    std::map<std::string, std::string> FilterPanelViewModel::columnFilters() const {
        return columnFilters_;
    }

    void FilterPanelViewModel::setColumnFilters(std::map<std::string, std::string> filters) {
        if (columnFilters_ == filters) {
            return;
        }
        columnFilters_ = std::move(filters);
        markActiveFiltersDirty();
        emit changed();
    }

    void FilterPanelViewModel::addColumnFilter() {
        const auto key = columnKey_.trimmed();
        const auto value = columnValue_.trimmed();
        if (!key.isEmpty() && !value.isEmpty()) {
            columnFilters_[key.toStdString()] = value.toStdString();
            columnValue_.clear();
            markActiveFiltersDirty();
            emit changed();
            emit applyRequested();
        }
    }

    void FilterPanelViewModel::resetFilters() {
        quickSector_.clear();
        columnKey_ = QString::fromStdString(domain::ColumnCatalog::defaultFilterColumnKey());
        columnValue_.clear();
        columnFilters_.clear();
        excludeScaSesSte_ = domain::kDefaultExcludeScaSesSte;
        markActiveFiltersDirty();
        emit changed();
        emit applyRequested();
    }

    void FilterPanelViewModel::rebuildActiveFilters() {
        const auto activeParts =
            domain::filterSummaryParts(quickSector_.trimmed().toStdString(), excludeScaSesSte_,
                                      columnFilters_);
        activeFilters_.clear();
        activeFilters_.reserve(static_cast<int>(activeParts.size()));
        for (const auto& filter : activeParts) {
            activeFilters_.push_back(QString::fromStdString(filter));
        }
        activeFilterSummary_ = QString::fromStdString(domain::joinFilterSummary(activeParts));
    }

    void FilterPanelViewModel::markActiveFiltersDirty() {
        activeFiltersStale_ = true;
    }

    void FilterPanelViewModel::ensureActiveFiltersUpToDate() const {
        if (!activeFiltersStale_) {
            return;
        }
        const_cast<FilterPanelViewModel*>(this)->rebuildActiveFilters();
        activeFiltersStale_ = false;
    }

} // namespace ssa::presentation
