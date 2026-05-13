#include "presentation/SsaTableColumnManager.h"

#include <QVariantMap>

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    void SsaTableColumnManager::setWidthOverrides(const std::map<std::string, int>& widths) {
        widthOverrides_.clear();
        widthOverrides_.reserve(widths.size());
        for (const auto& [column, width] : widths) {
            widthOverrides_.emplace(column, width);
        }
        rebuildWidthCache();
    }

    void SsaTableColumnManager::replace(std::vector<std::string> keys,
                                        std::vector<SsaDisplayColumn> displayColumns) {
        if (displayColumns.size() != keys.size()) {
            throw std::logic_error("table display columns do not match page columns");
        }

        std::vector<ColumnState> nextColumns;
        nextColumns.reserve(keys.size());
        for (std::size_t index = 0; index < keys.size(); ++index) {
            ColumnState column;
            column.key = std::move(keys[index]);
            column.display = std::move(displayColumns[index]);
            nextColumns.push_back(std::move(column));
        }
        columns_ = std::move(nextColumns);
        rebuildCaches();
    }

    bool SsaTableColumnManager::empty() const {
        return columns_.empty();
    }

    std::size_t SsaTableColumnManager::count() const {
        return columns_.size();
    }

    bool SsaTableColumnManager::hasColumn(const int column) const {
        return indexFor(column).has_value();
    }

    bool SsaTableColumnManager::hasSameKeys(const std::vector<std::string>& keys) const {
        if (columns_.size() != keys.size()) {
            return false;
        }
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (columns_[index].key != keys[index]) {
                return false;
            }
        }
        return true;
    }

    bool SsaTableColumnManager::hasSameMetadata(
        const std::vector<std::string>& keys,
        const std::vector<SsaDisplayColumn>& displayColumns) const {
        if (!hasSameKeys(keys) || displayColumns.size() != columns_.size()) {
            return false;
        }
        for (std::size_t index = 0; index < displayColumns.size(); ++index) {
            const auto& current = columns_[index].display;
            const auto& next = displayColumns[index];
            if (current.key != next.key || current.label != next.label ||
                current.type != next.type || current.defaultWidth != next.defaultWidth) {
                return false;
            }
        }
        return true;
    }

    QString SsaTableColumnManager::key(const int column) const {
        const auto index = indexFor(column);
        if (!index) {
            return {};
        }
        return QString::fromStdString(columns_[*index].key);
    }

    QString SsaTableColumnManager::label(const int column) const {
        const auto index = indexFor(column);
        if (!index) {
            return {};
        }
        return QString::fromStdString(columns_[*index].display.label);
    }

    int SsaTableColumnManager::width(const int column) const {
        const auto index = indexFor(column);
        if (!index) {
            return kFallbackTableColumnWidth;
        }
        return columns_[*index].width;
    }

    QVariantList SsaTableColumnManager::widths() const {
        return widthValues_;
    }

    QVariantList SsaTableColumnManager::tableColumns() const {
        return tableColumnValues_;
    }

    QStringList SsaTableColumnManager::keys() const {
        QStringList result;
        result.reserve(static_cast<qsizetype>(columns_.size()));
        for (const auto& column : columns_) {
            result.push_back(QString::fromStdString(column.key));
        }
        return result;
    }

    std::optional<std::size_t> SsaTableColumnManager::indexFor(const int column) const {
        if (column < 0) {
            return std::nullopt;
        }
        const auto index = static_cast<std::size_t>(column);
        if (index >= columns_.size()) {
            return std::nullopt;
        }
        return index;
    }

    void SsaTableColumnManager::rebuildCaches() {
        tableColumnValues_.clear();
        tableColumnValues_.reserve(static_cast<qsizetype>(columns_.size()));

        for (const auto& column : columns_) {
            QVariantMap item;
            item.insert("key", QString::fromStdString(column.key));
            item.insert("label", QString::fromStdString(column.display.label));
            tableColumnValues_.push_back(item);
        }
        rebuildWidthCache();
    }

    void SsaTableColumnManager::rebuildWidthCache() {
        widthValues_.clear();
        widthValues_.reserve(static_cast<qsizetype>(columns_.size()));
        for (auto& column : columns_) {
            const auto override = widthOverrides_.find(column.key);
            column.width =
                override != widthOverrides_.end() ? override->second : column.display.defaultWidth;
            widthValues_.push_back(column.width);
        }
    }

} // namespace ssa::presentation
