#pragma once

#include "domain/SsaTypes.h"

#include <QAbstractTableModel>
#include <QStringList>

namespace ssa::presentation {

    class SsaTableModel final : public QAbstractTableModel {
        Q_OBJECT

      public:
        explicit SsaTableModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                          int role) const override;

        void setPage(domain::SsaPageResult page, std::vector<std::string> columns);
        [[nodiscard]] const domain::SsaRecord* recordAt(int row) const;
        [[nodiscard]] QStringList columnKeys() const;

      private:
        std::vector<domain::SsaRecord> rows_;
        std::vector<std::string> columns_;
    };

} // namespace ssa::presentation
