#include "presentation/DetailsViewModel.h"

namespace ssa::presentation {

    DetailsViewModel::DetailsViewModel(QObject* parent) : QObject(parent), fields_(this) {}

    QString DetailsViewModel::title() const {
        return title_;
    }

    DetailsFieldsModel* DetailsViewModel::fields() {
        return &fields_;
    }

    int DetailsViewModel::fieldCount() const {
        return fields_.rowCount();
    }

    void DetailsViewModel::setRecord(const domain::SsaRecord& record) {
        const auto selectedSsa = record.valueOf(domain::kSsaNumberColumnKey);
        selectedSsa_ =
            QString::fromUtf8(selectedSsa.data(), static_cast<qsizetype>(selectedSsa.size()));
        if (selectedSsa_.isEmpty()) {
            clearRecord();
            return;
        }
        title_ = "SSA " + selectedSsa_;
        fields_.setRecord(record);
        emit changed();
    }

    void DetailsViewModel::clearRecord() {
        fields_.clear();
        selectedSsa_.clear();
        title_ = "Nenhuma SSA selecionada";
        emit changed();
    }

    QString DetailsViewModel::selectedSsa() const {
        return selectedSsa_;
    }

} // namespace ssa::presentation
