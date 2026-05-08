#include "presentation/DetailsViewModel.h"

#include "domain/ColumnCatalog.h"

#include <QVariantMap>

namespace ssa::presentation {

    DetailsViewModel::DetailsViewModel(QObject* parent) : QObject(parent) {}

    QString DetailsViewModel::title() const {
        return title_;
    }

    QVariantList DetailsViewModel::fields() const {
        return fields_;
    }

    void DetailsViewModel::setRecord(const domain::SsaRecord* record) {
        fields_.clear();
        selectedSsa_.clear();
        if (record == nullptr) {
            title_ = "Nenhuma SSA selecionada";
            emit changed();
            return;
        }

        selectedSsa_ = QString::fromStdString(record->valueOf("numero_ssa"));
        title_ = selectedSsa_.isEmpty() ? "Detalhes" : "SSA " + selectedSsa_;
        for (const auto& column : domain::ColumnCatalog::all()) {
            const auto value = record->valueOf(column.key);
            if (value.empty()) {
                continue;
            }
            QVariantMap field;
            field.insert("label", QString::fromStdString(column.label));
            field.insert("value", QString::fromStdString(value));
            fields_.push_back(field);
        }
        emit changed();
    }

    QString DetailsViewModel::selectedSsa() const {
        return selectedSsa_;
    }

} // namespace ssa::presentation
