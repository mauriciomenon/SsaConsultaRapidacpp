#pragma once

#include "domain/SsaTypes.h"

#include <QAbstractTableModel>
#include <QStringList>

namespace ssa::presentation {

    class SsaTableModel final : public QAbstractTableModel {
        Q_OBJECT
        Q_PROPERTY(int visibleColumnCount READ visibleColumnCount NOTIFY columnsChanged)

      public:
        explicit SsaTableModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                          int role) const override;

        void setPage(domain::SsaPageResult page, std::vector<std::string> columns);
        Q_INVOKABLE [[nodiscard]] int visibleColumnCount() const;
        Q_INVOKABLE [[nodiscard]] QString columnKey(int column) const;
        Q_INVOKABLE [[nodiscard]] QString columnLabel(int column) const;
        Q_INVOKABLE [[nodiscard]] int columnWidth(int column) const;
        [[nodiscard]] const domain::SsaRecord* recordAt(int row) const;
        [[nodiscard]] QStringList columnKeys() const;

      signals:
        void columnsChanged();

      private:
        std::vector<domain::SsaRecord> rows_;
        std::vector<std::string> columns_;
    };

} // namespace ssa::presentation
