#include "presentation/AdvancedMacroFilterViewModel.h"

#include "domain/SsaTypes.h"
#include "query/SsaQueryService.h"

#include <QDate>
#include <QVariantMap>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace ssa::presentation {

    namespace {
        constexpr auto kNone = "";
        constexpr auto kBaixar = "ssas_para_baixar";
        constexpr auto kExecutadasSetor = "ssas_executadas_setor";
        constexpr auto kExecutadasDivisao = "ssas_executadas_divisao";
        constexpr auto kStatusColumn = "situacao";
        constexpr auto kBaixarStatusFilter = "!SAD,!SCA,!SES,!STE";
        constexpr int kYearWeekTextLength = 6;
        constexpr int kMaxIsoYearSearchDays = 370;
        constexpr int kDaysPerWeek = 7;

        struct ReportKey {
            QString setor;
            QString semana;
            QString pessoa;

            bool operator<(const ReportKey& other) const {
                return std::tie(setor, semana, pessoa) <
                       std::tie(other.setor, other.semana, other.pessoa);
            }
        };

        bool isCurrentMonthWeek(const QString& weekText, const QDate& currentDate) {
            const auto trimmedWeek = weekText.trimmed();
            bool conversionOk = false;
            const int yearWeek = trimmedWeek.toInt(&conversionOk);
            if (!conversionOk || trimmedWeek.size() != kYearWeekTextLength) {
                return false;
            }
            const int isoYear = yearWeek / domain::kYearWeekMultiplier;
            const int isoWeek = yearWeek % domain::kYearWeekMultiplier;
            QDate monday{isoYear, 1, 1};
            for (int day = 0; day < kMaxIsoYearSearchDays; ++day) {
                int candidateIsoYear = 0;
                if (monday.weekNumber(&candidateIsoYear) == isoWeek &&
                    candidateIsoYear == isoYear) {
                    const auto weekStart = monday.addDays(1 - monday.dayOfWeek());
                    for (int offset = 0; offset < kDaysPerWeek; ++offset) {
                        const auto date = weekStart.addDays(offset);
                        if (date.month() == currentDate.month() &&
                            date.year() == currentDate.year()) {
                            return true;
                        }
                    }
                    return false;
                }
                monday = monday.addDays(1);
            }
            return false;
        }

        QVariantMap reportRowMap(const ReportKey& key, const std::set<QString>& ssas) {
            return QVariantMap{{"group", key.setor},
                               {"week", key.semana},
                               {"person", key.pessoa},
                               {"count", static_cast<int>(ssas.size())}};
        }

        domain::SsaPageRequest reportRequestFromState(const filterpanel::FilterPanelState& state) {
            domain::SsaPageRequest request;
            request.visibleColumns = {"numero_ssa", "setor_executor", "semana_executada",
                                      "responsavel_execucao"};
            request.quickSector = state.quickSector().trimmed().toStdString();
            request.excludeScaSesSte = state.excludeScaSesSte();
            request.columnFilters = state.columnFilters();
            request.advancedFilters = state.advancedFilters();
            request.pageSize = 0;
            return request;
        }
    } // namespace

    AdvancedMacroFilterViewModel::AdvancedMacroFilterViewModel(
        filterpanel::FilterPanelAdvancedState& advancedState,
        const filterpanel::FilterPanelState& filterState,
        std::shared_ptr<query::SsaQueryService> queryService, QObject* parent)
        : QObject(parent), advancedState_(advancedState), filterState_(filterState),
          queryService_(std::move(queryService)),
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

    const QVariantList& AdvancedMacroFilterViewModel::reportRows() const {
        return reportRows_;
    }

    void AdvancedMacroFilterViewModel::refreshFromState() {
        emit changed();
    }

    void AdvancedMacroFilterViewModel::applyBaixarPreset() {
        advancedState_.setTextFilter(QString::fromLatin1(kStatusColumn),
                                     QString::fromLatin1(kBaixarStatusFilter));
    }

    void AdvancedMacroFilterViewModel::buildExecutadasReport(const QString& value) {
        const bool byDivision = value == QString::fromLatin1(kExecutadasDivisao);
        reportTitle_ = byDivision ? tr("SSA Executadas Divisao") : tr("SSA Executadas Setor");
        reportRows_.clear();
        if (!queryService_) {
            reportText_ = tr("Fonte de dados indisponivel.");
            return;
        }

        std::map<ReportKey, std::set<QString>> grouped;
        const auto currentDate = QDate::currentDate();
        const auto result = queryService_->readAll(
            reportRequestFromState(filterState_), [&](const domain::SsaRecord& record) {
                const auto setor =
                    QString::fromStdString(std::string{record.valueOf("setor_executor")})
                        .trimmed()
                        .toUpper();
                const auto semana =
                    QString::fromStdString(std::string{record.valueOf("semana_executada")})
                        .trimmed();
                auto pessoa =
                    QString::fromStdString(std::string{record.valueOf("responsavel_execucao")})
                        .trimmed();
                const auto ssa =
                    QString::fromStdString(std::string{record.valueOf("numero_ssa")}).trimmed();
                if (setor.isEmpty() || semana.isEmpty() || ssa.isEmpty() ||
                    !isCurrentMonthWeek(semana, currentDate)) {
                    return std::optional<std::string>{};
                }
                if (pessoa.isEmpty()) {
                    pessoa = QStringLiteral("-");
                }
                const auto groupSetor = byDivision ? setor.left(3) : setor;
                grouped[{groupSetor, semana, pessoa}].insert(ssa);
                return std::optional<std::string>{};
            });
        if (!result.ok()) {
            reportText_ = QString::fromStdString(result.error);
            return;
        }
        for (const auto& [key, ssas] : grouped) {
            reportRows_.push_back(reportRowMap(key, ssas));
        }
        reportText_ = reportRows_.empty()
                          ? tr("Nenhuma SSA baixada no mes vigente para o recorte atual.")
                          : tr("%1 linhas agrupadas no mes vigente.").arg(reportRows_.size());
    }

    void AdvancedMacroFilterViewModel::clearReport() {
        reportTitle_.clear();
        reportText_.clear();
        reportRows_.clear();
    }

} // namespace ssa::presentation
