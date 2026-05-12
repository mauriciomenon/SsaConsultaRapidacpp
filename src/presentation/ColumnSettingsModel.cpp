#include "presentation/ColumnSettingsModel.h"

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <set>

namespace ssa::presentation {

    ColumnSettingsModel::ColumnSettingsModel(QObject* parent) : QAbstractListModel(parent) {
        for (const auto& column : domain::ColumnCatalog::all()) {
            columns_.push_back(ColumnItem{column.key, column.label, column.defaultVisible,
                                          column.defaultVisible, column.defaultWidth,
                                          column.defaultWidth});
        }
    }

    int ColumnSettingsModel::rowCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : static_cast<int>(columns_.size());
    }

    QVariant ColumnSettingsModel::data(const QModelIndex& index, const int role) const {
        if (!index.isValid() || index.row() < 0) {
            return {};
        }
        const auto row = static_cast<std::size_t>(index.row());
        if (row >= columns_.size()) {
            return {};
        }
        const auto& column = columns_[row];
        switch (role) {
        case KeyRole:
            return QString::fromStdString(column.key);
        case LabelRole:
            return QString::fromStdString(column.label);
        case VisibleRole:
            return column.visible;
        case WidthRole:
            return column.width;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> ColumnSettingsModel::roleNames() const {
        return {{KeyRole, "columnKey"},
                {LabelRole, "columnLabel"},
                {VisibleRole, "columnVisible"},
                {WidthRole, "columnWidth"}};
    }

    void ColumnSettingsModel::setColumnVisible(const int row, const bool visible) {
        if (row < 0) {
            return;
        }
        const auto index = static_cast<std::size_t>(row);
        if (index >= columns_.size() || columns_[index].visible == visible) {
            return;
        }
        if (!visible && visibleCount() == 1) {
            return;
        }
        columns_[index].visible = visible;
        emitRowChanged(row);
        emit changed();
    }

    void ColumnSettingsModel::setColumnWidth(const QString& columnKey, const int width) {
        const auto key = columnKey.toStdString();
        const auto item = std::ranges::find_if(
            columns_, [&key](const ColumnItem& column) { return column.key == key; });
        if (item == columns_.end()) {
            return;
        }
        const int bounded = std::clamp(width, 80, 520);
        if (item->width == bounded) {
            return;
        }
        item->width = bounded;
        emitRowChanged(static_cast<int>(std::distance(columns_.begin(), item)));
        emit changed();
    }

    void ColumnSettingsModel::resetDefaults() {
        for (auto& column : columns_) {
            column.visible = column.defaultVisible;
            column.width = column.defaultWidth;
        }
        emit dataChanged(index(0), index(rowCount() - 1));
        emit changed();
    }

    void ColumnSettingsModel::selectAll() {
        for (auto& column : columns_) {
            column.visible = true;
        }
        emit dataChanged(index(0), index(rowCount() - 1), {VisibleRole});
        emit changed();
    }

    void ColumnSettingsModel::applyPreferences(const std::vector<std::string>& visibleColumns,
                                               const std::map<std::string, int>& columnWidths) {
        std::set<std::string> visible(visibleColumns.begin(), visibleColumns.end());
        if (visible.empty()) {
            for (const auto& column : columns_) {
                if (column.defaultVisible) {
                    visible.insert(column.key);
                }
            }
        }

        for (auto& column : columns_) {
            column.visible = visible.contains(column.key);
            const auto width = columnWidths.find(column.key);
            column.width = width == columnWidths.end() ? column.defaultWidth
                                                       : std::clamp(width->second, 80, 520);
        }
        emit dataChanged(index(0), index(rowCount() - 1));
        emit changed();
    }

    std::vector<std::string> ColumnSettingsModel::visibleKeys() const {
        std::vector<std::string> keys;
        for (const auto& column : columns_) {
            if (column.visible) {
                keys.push_back(column.key);
            }
        }
        return keys;
    }

    std::map<std::string, int> ColumnSettingsModel::columnWidths() const {
        std::map<std::string, int> widths;
        for (const auto& column : columns_) {
            widths[column.key] = column.width;
        }
        return widths;
    }

    int ColumnSettingsModel::visibleCount() const {
        return static_cast<int>(std::ranges::count_if(
            columns_, [](const ColumnItem& column) { return column.visible; }));
    }

    void ColumnSettingsModel::emitRowChanged(const int row) {
        const QModelIndex itemIndex = index(row);
        emit dataChanged(itemIndex, itemIndex);
    }

} // namespace ssa::presentation
