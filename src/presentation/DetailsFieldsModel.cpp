#include "presentation/DetailsFieldsModel.h"

#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaRecordValueFormatter.h"

#include <QString>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace ssa::presentation {

    namespace {

        constexpr std::array<std::string_view, 29> kDetailsFieldOrder{{
            "numero_ssa",
            "situacao",
            "localizacao_codigo",
            "setor_emissor",
            "setor_executor",
            "qtd_derivadas",
            "derivada_de",
            "data_cadastro",
            "semana_cadastro",
            "descricao_ssa",
            "grau_prioridade_emissao",
            "grau_prioridade_planejamento",
            "solicitante",
            "responsavel_programacao",
            "responsavel_execucao",
            "descricao_localizacao",
            "equipamento",
            "servico_origem",
            "sistema_origem",
            "arquivo_origem",
            "execucao_simples",
            "semana_programada",
            "semana_executada",
            "status_execucao_prazo",
            "num_reprogramacoes",
            "total_de_reprogramacoes",
            "execucao_parcial",
            "justificativa",
            "parciais",
        }};

        std::size_t detailsFieldRank(const std::string_view key) {
            const auto it = std::find(kDetailsFieldOrder.begin(), kDetailsFieldOrder.end(), key);
            return it == kDetailsFieldOrder.end()
                       ? kDetailsFieldOrder.size()
                       : static_cast<std::size_t>(std::distance(kDetailsFieldOrder.begin(), it));
        }

    } // namespace

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
            if (field.key == "servico_origem") {
                return QStringLiteral("Servico");
            }
            if (field.key == "sistema_origem") {
                return QStringLiteral("Sistema");
            }
            if (field.key == "arquivo_origem") {
                return QStringLiteral("Arquivo");
            }
            if (field.key == "origem") {
                return QStringLiteral("Fonte");
            }
            return QString::fromStdString(SsaColumnDisplayCatalog{}.resolve(field.key).label);
        }
        if (role == ValueRole) {
            return SsaRecordValueFormatter::valueFor(field.value, column == columnsByKey_.end()
                                                                      ? domain::ColumnType::Text
                                                                      : column->second.type);
        }
        if (role == KeyRole) {
            return QString::fromStdString(field.key);
        }
        return {};
    }

    QHash<int, QByteArray> DetailsFieldsModel::roleNames() const {
        return {{LabelRole, "label"}, {ValueRole, "value"}, {KeyRole, "key"}};
    }

    void DetailsFieldsModel::setRecord(const domain::SsaRecord& record) {
        const auto recordFields = orderedNonEmptyFields(record.fields());
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
    DetailsFieldsModel::orderedNonEmptyFields(const RecordFields& recordFields) {
        RecordFields result;
        result.reserve(recordFields.size());
        for (const auto& field : recordFields) {
            if (field.key == "id") {
                continue;
            }
            if (!field.value.empty()) {
                result.push_back(field);
            }
        }
        std::stable_sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            return detailsFieldRank(left.key) < detailsFieldRank(right.key);
        });
        return result;
    }

    std::vector<DetailsFieldsModel::FieldEntry>
    DetailsFieldsModel::buildEntries(const RecordFields& recordFields) {
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
