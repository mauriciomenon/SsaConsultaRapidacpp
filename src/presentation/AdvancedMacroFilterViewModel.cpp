#include "presentation/AdvancedMacroFilterViewModel.h"

#include <QVariantMap>

namespace ssa::presentation {

    namespace {
        constexpr auto kNone = "";
        constexpr auto kBaixar = "ssas_para_baixar";
        constexpr auto kExecutadasSetor = "ssas_executadas_setor";
        constexpr auto kExecutadasDivisao = "ssas_executadas_divisao";
        constexpr auto kStatusColumn = "situacao";
        constexpr auto kBaixarStatusFilter = "!SAD,!SCA,!SES,!STE";
    } // namespace

    AdvancedMacroFilterViewModel::AdvancedMacroFilterViewModel(
        filterpanel::FilterPanelAdvancedState& state, QObject* parent)
        : QObject(parent), state_(state),
          options_{
              QVariantMap{{"label", tr("Nenhum")}, {"value", QString::fromLatin1(kNone)}},
              QVariantMap{{"label", tr("Baixar")}, {"value", QString::fromLatin1(kBaixar)}},
              QVariantMap{{"label", tr("SSA Executadas Setor")},
                          {"value", QString::fromLatin1(kExecutadasSetor)}},
              QVariantMap{{"label", tr("SSA Executadas Divisao")},
                          {"value", QString::fromLatin1(kExecutadasDivisao)}},
          } {}

    const QVariantList& AdvancedMacroFilterViewModel::options() const {
        return options_;
    }

    QString AdvancedMacroFilterViewModel::selectedMacro() const {
        return selectedMacro_;
    }

    void AdvancedMacroFilterViewModel::setSelectedMacro(const QString& value) {
        const auto normalized = value.trimmed();
        if (selectedMacro_ == normalized) {
            return;
        }
        selectedMacro_ = normalized;
        if (selectedMacro_ == QString::fromLatin1(kBaixar)) {
            applyBaixarPreset();
            clearReport();
        } else if (selectedMacro_ == QString::fromLatin1(kExecutadasSetor) ||
                   selectedMacro_ == QString::fromLatin1(kExecutadasDivisao)) {
            buildExecutadasReport(selectedMacro_);
            selectedMacro_.clear();
        } else {
            clearReport();
        }
        emit changed();
    }

    QString AdvancedMacroFilterViewModel::reportTitle() const {
        return reportTitle_;
    }

    QString AdvancedMacroFilterViewModel::reportText() const {
        return reportText_;
    }

    void AdvancedMacroFilterViewModel::refreshFromState() {
        emit changed();
    }

    void AdvancedMacroFilterViewModel::applyBaixarPreset() {
        state_.setTextFilter(QString::fromLatin1(kStatusColumn),
                             QString::fromLatin1(kBaixarStatusFilter));
    }

    void AdvancedMacroFilterViewModel::buildExecutadasReport(const QString& value) {
        const bool byDivision = value == QString::fromLatin1(kExecutadasDivisao);
        reportTitle_ = byDivision ? tr("SSA Executadas Divisao") : tr("SSA Executadas Setor");
        reportText_ =
            byDivision
                ? tr("Relatorio agrupado por divisao requer fonte de dados completa; use a "
                     "selecao de divisao/setor e exporte o resultado filtrado neste slice.")
                : tr("Relatorio agrupado por setor requer fonte de dados completa; use a selecao "
                     "de setor e exporte o resultado filtrado neste slice.");
    }

    void AdvancedMacroFilterViewModel::clearReport() {
        reportTitle_.clear();
        reportText_.clear();
    }

} // namespace ssa::presentation
