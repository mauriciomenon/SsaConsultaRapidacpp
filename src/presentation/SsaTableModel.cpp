#include "presentation/SsaTableModel.h"

#include "domain/ColumnCatalog.h"

namespace ssa::presentation {

    SsaTableModel::SsaTableModel(QObject* parent) : QAbstractTableModel(parent) {}

    int SsaTableModel::rowCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }

    int SsaTableModel::columnCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : static_cast<int>(columns_.size());
    }

    QVariant SsaTableModel::data(const QModelIndex& index, const int role) const {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return {};
        }
        const int column = index.column();
        const auto* record = recordAt(index.row());
        if (record == nullptr || column < 0) {
            return {};
        }
        const auto columnIndex = static_cast<std::size_t>(column);
        if (columnIndex >= columns_.size()) {
            return {};
        }
        return QString::fromStdString(record->valueOf(columns_[columnIndex]));
    }

    QVariant SsaTableModel::headerData(const int section, const Qt::Orientation orientation,
                                       const int role) const {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0) {
            return {};
        }
        const auto sectionIndex = static_cast<std::size_t>(section);
        if (sectionIndex >= columns_.size()) {
            return {};
        }
        const auto column = domain::ColumnCatalog::find(columns_[sectionIndex]);
        return QString::fromStdString(column ? column->label : columns_[sectionIndex]);
    }

    void SsaTableModel::setPage(domain::SsaPageResult page, std::vector<std::string> columns) {
        beginResetModel();
        rows_ = std::move(page.rows);
        columns_ = std::move(columns);
        endResetModel();
    }

    const domain::SsaRecord* SsaTableModel::recordAt(const int row) const {
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
        QStringList keys;
        for (const auto& column : columns_) {
            keys.push_back(QString::fromStdString(column));
        }
        return keys;
    }

} // namespace ssa::presentation
