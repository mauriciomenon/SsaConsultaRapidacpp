#include "presentation/ColumnFilterViewModel.h"

#include "domain/ColumnCatalog.h"

namespace ssa::presentation {

    ColumnFilterViewModel::ColumnFilterViewModel(filterpanel::FilterPanelState& state,
                                                 QObject* parent)
        : QObject(parent), state_(state) {
        loadRows();
        refreshRowValues();
    }

    QVariantList ColumnFilterViewModel::rows() const {
        return rows_;
    }

    int ColumnFilterViewModel::activeFilterCount() const {
        return activeFilterCount_;
    }

    bool ColumnFilterViewModel::applyFilterFor(const QString& key, const QString& value) {
        if (!state_.addColumnFilter(key, value)) {
            return false;
        }
        refreshFromState();
        emit stateChanged();
        emit applyRequested();
        return true;
    }

    bool ColumnFilterViewModel::clearFilterFor(const QString& key) {
        if (!state_.removeColumnFilter(key)) {
            return false;
        }
        refreshFromState();
        emit stateChanged();
        emit applyRequested();
        return true;
    }

    void ColumnFilterViewModel::refreshFromState() {
        refreshRowValues();
    }

    QVariantMap ColumnFilterViewModel::rowToMap(const RowData& row) {
        QVariantMap map;
        map.insert("key", row.key);
        map.insert("label", row.label);
        map.insert("value", row.value);
        return map;
    }

    void ColumnFilterViewModel::loadRows() {
        for (const auto& key : domain::ColumnCatalog::orderedFilterColumnKeys()) {
            const auto* column = domain::ColumnCatalog::find(key);
            if (column == nullptr) {
                continue;
            }
            RowData row{column->key, QString::fromStdString(column->key),
                        QString::fromStdString(column->label), QString{}};
            rowData_.push_back(row);
            rows_.push_back(rowToMap(row));
        }
    }

    void ColumnFilterViewModel::refreshRowValues() {
        const auto& activeColumnFilters = state_.columnFilters();
        const auto nextActiveFilterCount = static_cast<int>(activeColumnFilters.size());

        bool changed = false;
        for (std::size_t index = 0; index < rowData_.size(); ++index) {
            auto& row = rowData_[index];
            const auto activeValue = activeColumnFilters.find(row.keyStd);
            const auto nextValue = activeValue == activeColumnFilters.end()
                                       ? QString{}
                                       : QString::fromStdString(activeValue->second);
            if (row.value == nextValue) {
                continue;
            }
            row.value = nextValue;
            rows_[static_cast<qsizetype>(index)] = rowToMap(row);
            changed = true;
        }
        if (changed) {
            emit rowsChanged();
        }
        if (activeFilterCount_ != nextActiveFilterCount) {
            activeFilterCount_ = nextActiveFilterCount;
            emit activeFilterCountChanged();
        }
    }

} // namespace ssa::presentation
