#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <QAbstractListModel>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ssa::presentation {

    class DetailsFieldsModel final : public QAbstractListModel {
        Q_OBJECT

      public:
        enum Role : std::uint16_t { LabelRole = Qt::UserRole + 1, ValueRole, KeyRole };

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

        [[nodiscard]] static RecordFields orderedNonEmptyFields(const RecordFields& recordFields);
        [[nodiscard]] static std::vector<FieldEntry> buildEntries(const RecordFields& recordFields);
        [[nodiscard]] bool hasSameSchema(const RecordFields& recordFields) const;
        void updateValues(const RecordFields& recordFields);

        std::vector<FieldEntry> fields_;
        std::unordered_map<std::string, domain::ColumnDef> columnsByKey_;
    };

} // namespace ssa::presentation
