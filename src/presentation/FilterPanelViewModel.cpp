#include "presentation/FilterPanelViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <utility>

namespace ssa::presentation {

    FilterPanelViewModel::FilterPanelViewModel(QObject* parent) : QObject(parent) {
        for (const auto& key : domain::ColumnCatalog::filterColumnKeys()) {
            filterColumnKeys_.push_back(QString::fromStdString(key));
        }
        columnKey_ = QString::fromStdString(domain::ColumnCatalog::defaultFilterColumnKey());
    }

    QString FilterPanelViewModel::quickSector() const {
        return quickSector_;
    }

    void FilterPanelViewModel::setQuickSector(const QString& value) {
        if (quickSector_ == value) {
            return;
        }
        quickSector_ = value;
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
        QStringList filters;
        if (!quickSector_.trimmed().isEmpty()) {
            filters.push_back("setor_executor:" + quickSector_.trimmed());
        }
        if (excludeScaSesSte_) {
            filters.push_back(QString::fromUtf8(domain::kScaSesSteExclusionSummary.data(),
                                                domain::kScaSesSteExclusionSummary.size()));
        }
        for (const auto& [key, value] : columnFilters_) {
            QString filter = QString::fromStdString(key);
            filter += ":";
            filter += QString::fromStdString(value);
            filters.push_back(filter);
        }
        return filters;
    }

    std::map<std::string, std::string> FilterPanelViewModel::columnFilters() const {
        return columnFilters_;
    }

    void FilterPanelViewModel::setColumnFilters(std::map<std::string, std::string> filters) {
        if (columnFilters_ == filters) {
            return;
        }
        columnFilters_ = std::move(filters);
        emit changed();
    }

    void FilterPanelViewModel::addColumnFilter() {
        const auto key = columnKey_.trimmed();
        const auto value = columnValue_.trimmed();
        if (!key.isEmpty() && !value.isEmpty()) {
            columnFilters_[key.toStdString()] = value.toStdString();
            columnValue_.clear();
            emit changed();
            emit applyRequested();
        }
    }

    void FilterPanelViewModel::resetFilters() {
        quickSector_.clear();
        columnKey_ = QString::fromStdString(domain::ColumnCatalog::defaultFilterColumnKey());
        columnValue_.clear();
        columnFilters_.clear();
        excludeScaSesSte_ = true;
        emit changed();
        emit applyRequested();
    }

} // namespace ssa::presentation
