#include "presentation/ColumnSettingsModel.h"

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <set>

namespace ssa::presentation {
    namespace {
        constexpr int kMinColumnWidth = 80;
        constexpr int kMaxColumnWidth = 520;
    } // namespace

    ColumnSettingsModel::ColumnSettingsModel(QObject* parent) : QAbstractListModel(parent) {
        for (const auto& column : domain::ColumnCatalog::all()) {
            columns_.push_back(ColumnItem{column.key, column.label, column.defaultVisible,
                                          column.defaultVisible, column.defaultWidth,
                                          column.defaultWidth});
            if (column.defaultVisible) {
                ++visibleCount_;
            }
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
        case ToggleEnabledRole:
            return !column.visible || visibleCount() != 1;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> ColumnSettingsModel::roleNames() const {
        return {{KeyRole, "columnKey"},
                {LabelRole, "columnLabel"},
                {VisibleRole, "columnVisible"},
                {WidthRole, "columnWidth"},
                {ToggleEnabledRole, "columnToggleEnabled"}};
    }

    int ColumnSettingsModel::minColumnWidth() const {
        return kMinColumnWidth;
    }

    int ColumnSettingsModel::maxColumnWidth() const {
        return kMaxColumnWidth;
    }

    bool ColumnSettingsModel::setColumnVisible(const int row, const bool visible) {
        if (row < 0) {
            return false;
        }
        const auto index = static_cast<std::size_t>(row);
        if (index >= columns_.size()) {
            return false;
        }
        if (columns_[index].visible == visible) {
            return true;
        }
        const int previousVisibleCount = visibleCount();
        if (!visible && previousVisibleCount == 1) {
            return false;
        }
        columns_[index].visible = visible;
        visibleCount_ += visible ? 1 : -1;
        emit dataChanged(this->index(row), this->index(row), {VisibleRole, ToggleEnabledRole});
        if ((previousVisibleCount <= 1) != (visibleCount() <= 1)) {
            emit dataChanged(this->index(0), this->index(rowCount() - 1), {ToggleEnabledRole});
        }
        emit changed();
        return true;
    }

    bool ColumnSettingsModel::setColumnVisibleByKey(const QString& columnKey, const bool visible) {
        const auto key = columnKey.toStdString();
        const auto item = std::ranges::find_if(
            columns_, [&key](const ColumnItem& column) { return column.key == key; });
        if (item == columns_.end()) {
            return false;
        }
        return setColumnVisible(static_cast<int>(std::distance(columns_.begin(), item)), visible);
    }

    void ColumnSettingsModel::setColumnWidth(const QString& columnKey, const int width) {
        const auto key = columnKey.toStdString();
        const auto item = std::ranges::find_if(
            columns_, [&key](const ColumnItem& column) { return column.key == key; });
        if (item == columns_.end()) {
            return;
        }
        const int bounded = std::clamp(width, kMinColumnWidth, kMaxColumnWidth);
        if (item->width == bounded) {
            return;
        }
        item->width = bounded;
        emitRowChanged(static_cast<int>(std::distance(columns_.begin(), item)));
        emit changed();
    }

    void ColumnSettingsModel::resetDefaults() {
        visibleCount_ = 0;
        for (auto& column : columns_) {
            column.visible = column.defaultVisible;
            column.width = column.defaultWidth;
            if (column.visible) {
                ++visibleCount_;
            }
        }
        emit dataChanged(index(0), index(rowCount() - 1),
                         {VisibleRole, WidthRole, ToggleEnabledRole});
        emit changed();
    }

    void ColumnSettingsModel::selectAll() {
        for (auto& column : columns_) {
            column.visible = true;
        }
        visibleCount_ = static_cast<int>(columns_.size());
        emit dataChanged(index(0), index(rowCount() - 1), {VisibleRole, ToggleEnabledRole});
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

        visibleCount_ = 0;
        for (auto& column : columns_) {
            column.visible = visible.contains(column.key);
            if (column.visible) {
                ++visibleCount_;
            }
            const auto width = columnWidths.find(column.key);
            column.width = width == columnWidths.end()
                               ? column.defaultWidth
                               : std::clamp(width->second, kMinColumnWidth, kMaxColumnWidth);
        }
        if (visibleCount() == 0 && !columns_.empty()) {
            columns_.front().visible = true;
            visibleCount_ = 1;
        }
        emit dataChanged(index(0), index(rowCount() - 1),
                         {VisibleRole, WidthRole, ToggleEnabledRole});
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
        return visibleCount_;
    }

    void ColumnSettingsModel::emitRowChanged(const int row) {
        const QModelIndex itemIndex = index(row);
        emit dataChanged(itemIndex, itemIndex, {WidthRole});
    }

} // namespace ssa::presentation
