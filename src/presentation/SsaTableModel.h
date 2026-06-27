#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableColumnManager.h"
#include "presentation/SsaTableDisplayCache.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <QVariantList>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ssa::presentation {

    class SsaTableModel final : public QAbstractTableModel {
        Q_OBJECT
        Q_PROPERTY(int visibleColumnCount READ visibleColumnCount NOTIFY columnsChanged)
        Q_PROPERTY(QVariantList columnWidths READ columnWidths NOTIFY columnsChanged)
        Q_PROPERTY(QVariantList tableColumns READ tableColumns NOTIFY columnsChanged)
        Q_PROPERTY(int fallbackColumnWidth READ fallbackColumnWidth CONSTANT)

      public:
        explicit SsaTableModel(std::string idColumnKey, QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                          int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setPage(domain::SsaPageResult page, std::vector<std::string> columns,
                     std::vector<SsaDisplayColumn> displayColumns,
                     SsaTableDisplayValues displayValues);
        void setColumnWidths(const std::map<std::string, int>& widths);
        Q_INVOKABLE [[nodiscard]] int visibleColumnCount() const;
        Q_INVOKABLE [[nodiscard]] QString columnKey(int column) const;
        Q_INVOKABLE [[nodiscard]] QString columnLabel(int column) const;
        Q_INVOKABLE [[nodiscard]] int columnWidth(int column) const;
        Q_INVOKABLE [[nodiscard]] QVariantList columnWidths() const;
        Q_INVOKABLE [[nodiscard]] QVariantList tableColumns() const;
        Q_INVOKABLE [[nodiscard]] int fallbackColumnWidth() const;
        [[nodiscard]] std::optional<domain::SsaRecord> recordAt(int row) const;
        [[nodiscard]] QStringList columnKeys() const;

      signals:
        void columnsChanged();

      private:
        [[nodiscard]] bool hasColumn(int column) const;
        [[nodiscard]] bool
        canUpdateRowsWithoutReset(const std::vector<std::string>& columns,
                                  const std::vector<SsaDisplayColumn>& displayColumns,
                                  std::size_t nextRowCount) const;

        std::vector<domain::SsaRecord> rows_;
        SsaTableColumnManager columns_;
        SsaTableDisplayCache displayCache_;
        std::string idColumnKey_;
    };

} // namespace ssa::presentation
