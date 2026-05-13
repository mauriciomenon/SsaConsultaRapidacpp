#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "presentation/SsaRecordValueFormatter.h"

#include <QAbstractListModel>

#include <string>
#include <unordered_map>
#include <vector>

namespace ssa::presentation {

    class DetailsFieldsModel final : public QAbstractListModel {
        Q_OBJECT

      public:
        enum Role { LabelRole = Qt::UserRole + 1, ValueRole };

        explicit DetailsFieldsModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setRecord(const domain::SsaRecord& record);
        void clear();

      private:
        struct FieldEntry {
            std::string key;
            std::string value;
        };
        using RecordFields = std::vector<domain::SsaRecord::FieldView>;

        [[nodiscard]] RecordFields nonEmptyFields(const RecordFields& recordFields) const;
        [[nodiscard]] std::vector<FieldEntry> buildEntries(const RecordFields& recordFields) const;
        [[nodiscard]] bool hasSameSchema(const RecordFields& recordFields) const;
        void updateValues(const RecordFields& recordFields);

        std::vector<FieldEntry> fields_;
        std::unordered_map<std::string, domain::ColumnDef> columnsByKey_;
        SsaRecordValueFormatter valueFormatter_;
    };

} // namespace ssa::presentation
