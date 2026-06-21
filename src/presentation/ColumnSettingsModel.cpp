#include "presentation/ColumnSettingsModel.h"

#include "domain/ColumnCatalog.h"

#include <algorithm>
#include <set>
#include <utility>

namespace ssa::presentation {
    namespace {
        constexpr int kMinColumnWidth = 80;
        constexpr int kMaxColumnWidth = 520;

        [[nodiscard]] std::string toLower(std::string text) {
            return QString::fromStdString(std::move(text)).toLower().toStdString();
        }

    } // namespace

    ColumnSettingsModel::ColumnSettingsModel(QObject* parent) : QAbstractListModel(parent) {
        for (const auto& column : domain::ColumnCatalog::all()) {
            columns_.push_back(ColumnItem{column.key, column.label, toLower(column.key),
                                          toLower(column.label), column.defaultVisible,
                                          column.defaultVisible, column.defaultWidth,
                                          column.defaultWidth});
            if (column.defaultVisible) {
                ++visibleCount_;
            }
        }
        rebuildFilteredRows();
    }

    int ColumnSettingsModel::rowCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : static_cast<int>(filteredRows_.size());
    }

    QVariant ColumnSettingsModel::data(const QModelIndex& index, const int role) const {
        if (!index.isValid() || index.row() < 0) {
            return {};
        }
        const auto row = sourceRowFromModelRow(index.row());
        if (row < 0) {
            return {};
        }
        const auto& column = columns_[static_cast<std::size_t>(row)];
        switch (role) {
        case KeyRole:
            return QString::fromStdString(column.key);
        case LabelRole:
            return QString::fromStdString(column.label);
        case VisibleRole:
            return column.visible;
        case WidthRole:
            return column.width;
        case VisibilityChangeEnabledRole:
            return !column.visible || visibleCount() > 1;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> ColumnSettingsModel::roleNames() const {
        return {{KeyRole, "columnKey"},
                {LabelRole, "columnLabel"},
                {VisibleRole, "columnVisible"},
                {WidthRole, "columnWidth"},
                {VisibilityChangeEnabledRole, "columnVisibilityChangeEnabled"}};
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
        const auto sourceRow = sourceRowFromModelRow(row);
        if (sourceRow < 0) {
            return false;
        }
        return setColumnVisibleBySourceRow(static_cast<std::size_t>(sourceRow), visible);
    }

    bool ColumnSettingsModel::setColumnVisibleBySourceRow(const std::size_t sourceRow,
                                                          const bool visible) {
        if (sourceRow >= columns_.size()) {
            return false;
        }
        auto& column = columns_[sourceRow];
        if (column.visible == visible) {
            return true;
        }
        const int previousVisibleCount = visibleCount();
        if (!visible && previousVisibleCount == 1) {
            const auto modelRow = modelRowFromSourceRow(sourceRow);
            if (modelRow >= 0) {
                emit dataChanged(this->index(modelRow), this->index(modelRow), {VisibleRole});
            }
            return false;
        }
        column.visible = visible;
        visibleCount_ += visible ? 1 : -1;
        const auto modelRow = modelRowFromSourceRow(sourceRow);
        if (modelRow >= 0) {
            emit dataChanged(this->index(modelRow), this->index(modelRow),
                             {VisibleRole, VisibilityChangeEnabledRole});
        }
        if ((previousVisibleCount <= 1) != (visibleCount() <= 1) && rowCount() > 0) {
            emit dataChanged(this->index(0), this->index(rowCount() - 1),
                             {VisibilityChangeEnabledRole});
        }
        emit changed();
        return true;
    }

    std::vector<ColumnSettingsModel::ColumnItem>::iterator
    ColumnSettingsModel::findColumn(const std::string_view key) {
        return std::ranges::find_if(columns_,
                                    [&key](const ColumnItem& column) { return column.key == key; });
    }

    std::vector<ColumnSettingsModel::ColumnItem>::const_iterator
    ColumnSettingsModel::findColumn(const std::string_view key) const {
        return std::ranges::find_if(columns_,
                                    [&key](const ColumnItem& column) { return column.key == key; });
    }

    bool ColumnSettingsModel::setColumnVisibleByKey(const QString& columnKey, const bool visible) {
        const auto key = columnKey.toStdString();
        const auto item = findColumn(key);
        if (item == columns_.end()) {
            return false;
        }
        return setColumnVisibleBySourceRow(
            static_cast<std::size_t>(std::distance(columns_.begin(), item)), visible);
    }

    void ColumnSettingsModel::setColumnWidth(const QString& columnKey, const int width) {
        const auto key = columnKey.toStdString();
        const auto item = findColumn(key);
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

    bool ColumnSettingsModel::moveColumn(const int fromRow, const int toRow) {
        if (fromRow < 0 || toRow < 0 || fromRow == toRow) {
            return false;
        }
        const auto fromSource = sourceRowFromModelRow(fromRow);
        const auto toSource = sourceRowFromModelRow(toRow);
        if (fromSource < 0 || toSource < 0) {
            return false;
        }
        const auto fromIdx = static_cast<std::size_t>(fromSource);
        const auto toIdx = static_cast<std::size_t>(toSource);

        beginMoveRows(QModelIndex(), fromRow, fromRow, toRow);
        if (fromIdx < toIdx) {
            std::rotate(columns_.begin() + static_cast<std::ptrdiff_t>(fromIdx),
                        columns_.begin() + static_cast<std::ptrdiff_t>(fromIdx + 1),
                        columns_.begin() + static_cast<std::ptrdiff_t>(toIdx + 1));
        } else {
            std::rotate(columns_.begin() + static_cast<std::ptrdiff_t>(toIdx),
                        columns_.begin() + static_cast<std::ptrdiff_t>(fromIdx),
                        columns_.begin() + static_cast<std::ptrdiff_t>(fromIdx + 1));
        }
        rebuildFilteredRows();
        endMoveRows();
        emit changed();
        return true;
    }

    void ColumnSettingsModel::setFilterText(const QString& filterText) {
        const auto normalized = filterText.toLower().toStdString();
        if (normalized == filterTextLower_) {
            return;
        }
        beginResetModel();
        filterTextLower_ = normalized;
        rebuildFilteredRows();
        endResetModel();
    }

    void ColumnSettingsModel::resetDefaults() {
        beginResetModel();
        visibleCount_ = 0;
        for (auto& column : columns_) {
            column.visible = column.defaultVisible;
            column.width = column.defaultWidth;
            if (column.visible) {
                ++visibleCount_;
            }
        }
        rebuildFilteredRows();
        endResetModel();
        emit changed();
    }

    void ColumnSettingsModel::selectAll() {
        beginResetModel();
        for (auto& column : columns_) {
            column.visible = true;
        }
        visibleCount_ = static_cast<int>(columns_.size());
        rebuildFilteredRows();
        endResetModel();
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

        std::vector<ColumnItem> reordered;
        reordered.reserve(columns_.size());
        for (const auto& key : visibleColumns) {
            auto it = std::ranges::find_if(columns_,
                                           [&key](const ColumnItem& c) { return c.key == key; });
            if (it != columns_.end()) {
                reordered.push_back(*it);
            }
        }
        for (const auto& column : columns_) {
            if (visible.find(column.key) == visible.end()) {
                reordered.push_back(column);
            }
        }
        if (reordered.size() != columns_.size()) {
            reordered = columns_;
        }

        beginResetModel();
        columns_ = std::move(reordered);
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
        rebuildFilteredRows();
        endResetModel();
        emit changed();
    }

    void ColumnSettingsModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        if (snapshot.visibleColumns.empty() && snapshot.columnWidths.empty()) {
            resetDefaults();
            return;
        }

        auto visibleColumns = snapshot.visibleColumns;
        if (visibleColumns.empty()) {
            visibleColumns = visibleKeys();
        }
        auto columnWidths = snapshot.columnWidths;
        if (columnWidths.empty()) {
            columnWidths = this->columnWidths();
        }
        applyPreferences(visibleColumns, columnWidths);
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

    bool ColumnSettingsModel::matchesFilter(const ColumnItem& column) const {
        if (filterTextLower_.empty()) {
            return true;
        }
        return column.keyLower.find(filterTextLower_) != std::string::npos ||
               column.labelLower.find(filterTextLower_) != std::string::npos;
    }

    int ColumnSettingsModel::sourceRowFromModelRow(const int modelRow) const {
        if (modelRow < 0 || modelRow >= static_cast<int>(filteredRows_.size())) {
            return -1;
        }
        return filteredRows_[static_cast<std::size_t>(modelRow)];
    }

    int ColumnSettingsModel::modelRowFromSourceRow(const std::size_t sourceRow) const {
        if (sourceRow >= columns_.size()) {
            return -1;
        }
        const int modelRow = sourceToModelRow_[sourceRow];
        if (modelRow < 0) {
            return -1;
        }
        return modelRow;
    }

    void ColumnSettingsModel::rebuildFilteredRows() {
        filteredRows_.clear();
        filteredRows_.reserve(columns_.size());
        sourceToModelRow_.assign(columns_.size(), -1);
        for (std::size_t idx = 0; idx < columns_.size(); ++idx) {
            if (matchesFilter(columns_[idx])) {
                sourceToModelRow_[idx] = static_cast<int>(filteredRows_.size());
                filteredRows_.push_back(static_cast<int>(idx));
            }
        }
    }

    void ColumnSettingsModel::emitRowChanged(const int row) {
        const int modelRow = modelRowFromSourceRow(static_cast<std::size_t>(row));
        if (modelRow < 0) {
            return;
        }
        const QModelIndex itemIndex = index(modelRow);
        emit dataChanged(itemIndex, itemIndex, {WidthRole});
    }

} // namespace ssa::presentation
