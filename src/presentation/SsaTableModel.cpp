#include "presentation/SsaTableModel.h"

#include <stdexcept>
#include <utility>

namespace ssa::presentation {

    SsaTableModel::SsaTableModel(std::string idColumnKey, QObject* parent)
        : QAbstractTableModel(parent), idColumnKey_(std::move(idColumnKey)) {}

    int SsaTableModel::rowCount(const QModelIndex& parent) const {
        if (parent.isValid() || columns_.empty()) {
            return 0;
        }
        return static_cast<int>(rows_.size());
    }

    int SsaTableModel::columnCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : static_cast<int>(columns_.count());
    }

    QVariant SsaTableModel::data(const QModelIndex& index, const int role) const {
        if (!index.isValid()) {
            return {};
        }
        if (role != Qt::DisplayRole && role != Qt::ToolTipRole) {
            return {};
        }
        const int column = index.column();
        if (index.row() < 0 || column < 0) {
            return {};
        }
        const auto columnIndex = static_cast<std::size_t>(column);
        const auto rowIndex = static_cast<std::size_t>(index.row());
        if (rowIndex >= rows_.size()) {
            return {};
        }
        if (columnIndex >= columns_.count()) {
            return {};
        }
        return displayCache_.value(rowIndex, columnIndex);
    }

    QVariant SsaTableModel::headerData(const int section, const Qt::Orientation orientation,
                                       const int role) const {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0) {
            return {};
        }
        const auto sectionIndex = static_cast<std::size_t>(section);
        if (sectionIndex >= columns_.count()) {
            return {};
        }
        return columns_.label(section);
    }

    QHash<int, QByteArray> SsaTableModel::roleNames() const {
        return {{Qt::DisplayRole, "displayValue"}};
    }

    void SsaTableModel::setPage(domain::SsaPageResult page, std::vector<std::string> columns,
                                std::vector<SsaDisplayColumn> displayColumns,
                                SsaTableDisplayValues displayValues) {
        auto nextRows = std::move(page.rows);
        if (displayColumns.size() != columns.size()) {
            throw std::logic_error("table display columns do not match page columns");
        }
        if (displayValues.rowCount != nextRows.size() ||
            displayValues.columnCount != columns.size() || !displayValues.hasValidShape()) {
            throw std::logic_error("table display values do not match page shape");
        }
        if (canUpdateRowsWithoutReset(columns, displayColumns, nextRows.size())) {
            rows_ = std::move(nextRows);
            displayCache_.replace(std::move(displayValues));
            const int rows = rowCount();
            const int displayedColumns = columnCount();
            if (rows > 0 && displayedColumns > 0) {
                emit dataChanged(index(0, 0), index(rows - 1, displayedColumns - 1),
                                 {Qt::DisplayRole, Qt::ToolTipRole});
            }
            return;
        }

        if (rows_.size() == nextRows.size() && columns_.hasSameKeys(columns)) {
            rows_ = std::move(nextRows);
            columns_.replace(std::move(columns), std::move(displayColumns));
            displayCache_.replace(std::move(displayValues));
            if (!columns_.empty()) {
                emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
            }
            emit columnsChanged();
            const int rows = rowCount();
            const int columnTotal = columnCount();
            if (rows > 0 && columnTotal > 0) {
                emit dataChanged(index(0, 0), index(rows - 1, columnTotal - 1),
                                 {Qt::DisplayRole, Qt::ToolTipRole});
            }
            return;
        }

        const bool columnKeysChanged = !columns_.hasSameKeys(columns);
        beginResetModel();
        rows_ = std::move(nextRows);
        columns_.replace(std::move(columns), std::move(displayColumns));
        displayCache_.replace(std::move(displayValues));
        endResetModel();
        if (columnKeysChanged) {
            emit columnsChanged();
        }
    }

    void SsaTableModel::setColumnWidths(const std::map<std::string, int>& widths) {
        columns_.setWidthOverrides(widths);
        if (!columns_.empty()) {
            emit columnsChanged();
        }
    }

    QString SsaTableModel::columnKey(const int column) const {
        if (!hasColumn(column)) {
            return {};
        }
        return columns_.key(column);
    }

    QString SsaTableModel::columnLabel(const int column) const {
        if (!hasColumn(column)) {
            return {};
        }
        return columns_.label(column);
    }

    int SsaTableModel::columnWidth(const int column) const {
        if (!hasColumn(column)) {
            return kFallbackTableColumnWidth;
        }
        return columns_.width(column);
    }

    QVariantList SsaTableModel::columnWidths() const {
        return columns_.widths();
    }

    QVariantList SsaTableModel::tableColumns() const {
        return columns_.tableColumns();
    }

    int SsaTableModel::fallbackColumnWidth() const {
        return kFallbackTableColumnWidth;
    }

    QString SsaTableModel::ssaNumberAt(const int row) const {
        const auto* record = recordAt(row);
        if (record == nullptr) {
            return {};
        }
        const auto value = record->valueOf(domain::kSsaNumberColumnKey);
        return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    }

    QString SsaTableModel::rowText(const int row) const {
        if (row < 0 || row >= static_cast<int>(rows_.size())) {
            return {};
        }

        QStringList values;
        values.reserve(static_cast<int>(columns_.count()));
        for (std::size_t column = 0; column < columns_.count(); ++column) {
            values.push_back(displayCache_.value(static_cast<std::size_t>(row), column).toString());
        }
        return values.join(QStringLiteral("\t"));
    }

    bool SsaTableModel::hasColumn(const int column) const {
        if (column < 0) {
            return false;
        }
        return columns_.hasColumn(column);
    }

    bool
    SsaTableModel::canUpdateRowsWithoutReset(const std::vector<std::string>& columns,
                                             const std::vector<SsaDisplayColumn>& displayColumns,
                                             const std::size_t nextRowCount) const {
        return rows_.size() == nextRowCount && columns_.hasSameMetadata(columns, displayColumns);
    }

    const domain::SsaRecord* SsaTableModel::recordAt(const int row) const noexcept {
        if (row < 0) {
            return nullptr;
        }
        const auto rowIndex = static_cast<std::size_t>(row);
        if (rowIndex >= rows_.size()) {
            return nullptr;
        }
        return &rows_[rowIndex];
    }

    QStringList SsaTableModel::columnKeys() const {
        return columns_.keys();
    }

} // namespace ssa::presentation
