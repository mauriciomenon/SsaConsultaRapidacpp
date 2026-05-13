#include "presentation/DetailsFieldsModel.h"

#include <QString>

namespace ssa::presentation {

    DetailsFieldsModel::DetailsFieldsModel(QObject* parent) : QAbstractListModel(parent) {
        for (const auto& column : domain::ColumnCatalog::all()) {
            columnsByKey_.emplace(column.key, column);
        }
    }

    int DetailsFieldsModel::rowCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : static_cast<int>(fields_.size());
    }

    QVariant DetailsFieldsModel::data(const QModelIndex& index, const int role) const {
        if (!index.isValid() || index.row() < 0) {
            return {};
        }
        const auto row = static_cast<std::size_t>(index.row());
        if (row >= fields_.size()) {
            return {};
        }
        const auto& field = fields_.at(row);
        const auto column = columnsByKey_.find(field.key);
        if (role == LabelRole) {
            return QString::fromStdString(column == columnsByKey_.end() ? field.key
                                                                        : column->second.label);
        }
        if (role == ValueRole) {
            return valueFormatter_.valueFor(field.value, column == columnsByKey_.end()
                                                             ? domain::ColumnType::Text
                                                             : column->second.type);
        }
        return {};
    }

    QHash<int, QByteArray> DetailsFieldsModel::roleNames() const {
        return {{LabelRole, "label"}, {ValueRole, "value"}};
    }

    void DetailsFieldsModel::setRecord(const domain::SsaRecord& record) {
        const auto recordFields = nonEmptyFields(record.fields());
        if (hasSameSchema(recordFields)) {
            updateValues(recordFields);
            if (!fields_.empty()) {
                emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {ValueRole});
            }
            return;
        }

        auto nextFields = buildEntries(recordFields);
        beginResetModel();
        fields_ = std::move(nextFields);
        endResetModel();
    }

    DetailsFieldsModel::RecordFields
    DetailsFieldsModel::nonEmptyFields(const RecordFields& recordFields) const {
        RecordFields result;
        result.reserve(recordFields.size());
        for (const auto& field : recordFields) {
            if (!field.value.empty()) {
                result.push_back(field);
            }
        }
        return result;
    }

    std::vector<DetailsFieldsModel::FieldEntry>
    DetailsFieldsModel::buildEntries(const RecordFields& recordFields) const {
        std::vector<FieldEntry> entries;
        entries.reserve(recordFields.size());
        for (const auto& [key, value] : recordFields) {
            const std::string keyText{key};
            const std::string valueText{value};
            entries.push_back({keyText, valueText});
        }
        return entries;
    }

    bool DetailsFieldsModel::hasSameSchema(const RecordFields& recordFields) const {
        if (fields_.size() != recordFields.size()) {
            return false;
        }
        for (std::size_t fieldIndex = 0; fieldIndex < fields_.size(); ++fieldIndex) {
            if (fields_[fieldIndex].key != recordFields[fieldIndex].key) {
                return false;
            }
        }
        return true;
    }

    void DetailsFieldsModel::updateValues(const RecordFields& recordFields) {
        for (std::size_t fieldIndex = 0; fieldIndex < fields_.size(); ++fieldIndex) {
            fields_[fieldIndex].value = recordFields[fieldIndex].value;
        }
    }

    void DetailsFieldsModel::clear() {
        beginResetModel();
        fields_.clear();
        endResetModel();
    }

} // namespace ssa::presentation
