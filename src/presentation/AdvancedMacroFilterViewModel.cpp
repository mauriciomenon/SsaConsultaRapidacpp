#include "presentation/AdvancedMacroFilterViewModel.h"

#include "domain/SsaTypes.h"
#include "query/SsaQueryService.h"

#include <QDate>
#include <QVariantMap>

#include <utility>

namespace ssa::presentation {

    namespace {
        constexpr auto kNone = "";
        constexpr auto kBaixar = "ssas_para_baixar";
        constexpr auto kExecutadasSetor = "ssas_executadas_setor";
        constexpr auto kExecutadasDivisao = "ssas_executadas_divisao";
        constexpr auto kStatusColumn = "situacao";
        constexpr auto kBaixarStatusFilter = "!SAD,!SCA,!SES,!STE";

        // Range of ISO year-weeks (YYYYWW) overlapping the month of currentDate.
        // The database restricts the set via semana_executada BETWEEN, so the
        // macro report never loads the full table just to filter it in memory.
        struct YearWeekRange {
            int start{0};
            int end{0};
        };

        // Returns the inclusive range of ISO year-weeks (YYYYWW) overlapping the
        // month of currentDate. The week encoding is year-major so BETWEEN works,
        // BUT only when both bounds share the same ISO year: if Dec 31 falls in
        // ISO week 1 of the next year, a naive [YYYY52, (YYYY+1)01] range would
        // cross the boundary and pull in next-January rows. We anchor both bounds
        // to the ISO year of a day safely inside the month (the 15th) so the
        // range stays within a single ISO year and covers every week that the
        // month touches.
        YearWeekRange executionWeekRangeForCurrentMonth(const QDate& currentDate) {
            const QDate firstOfMonth(currentDate.year(), currentDate.month(), 1);
            const QDate lastOfMonth(currentDate.year(), currentDate.month(),
                                    currentDate.daysInMonth());
            // A mid-month day always belongs to a week fully inside the month, so
            // its ISO year is the authoritative year for the whole range.
            int anchorIsoYear = 0;
            QDate(currentDate.year(), currentDate.month(), 15).weekNumber(&anchorIsoYear);

            int startIsoYear = 0;
            const int startWeek = firstOfMonth.weekNumber(&startIsoYear);
            int endIsoYear = 0;
            const int endWeek = lastOfMonth.weekNumber(&endIsoYear);

            // Boundary weeks (first/last day) may belong to an adjacent ISO year;
            // coerce them into the anchor year so the BETWEEN never crosses years.
            const auto resolveWeek = [anchorIsoYear](const int isoYear, const int week) {
                if (isoYear == anchorIsoYear) {
                    return anchorIsoYear * domain::kYearWeekMultiplier + week;
                }
                // Day fell into the tail (previous ISO year, high week number) or
                // the head (next ISO year, week 1) of the month. Clamp to the
                // anchor year's first/last week respectively.
                QDate lastOfAnchorYear(anchorIsoYear, 12, 28);
                int lastAnchorIsoYear = 0;
                const int lastAnchorWeek = lastOfAnchorYear.weekNumber(&lastAnchorIsoYear);
                return anchorIsoYear * domain::kYearWeekMultiplier +
                       (isoYear < anchorIsoYear ? 1 : lastAnchorWeek);
            };
            return YearWeekRange{resolveWeek(startIsoYear, startWeek),
                                 resolveWeek(endIsoYear, endWeek)};
        }

        QVariantMap reportRowMap(const application::ExecutadasReportRow& row) {
            return QVariantMap{{"group", QString::fromStdString(row.group)},
                               {"week", QString::fromStdString(row.week)},
                               {"person", QString::fromStdString(row.person)},
                               {"count", row.count}};
        }

        domain::SsaPageRequest reportRequestFromState(const filterpanel::FilterPanelState& state,
                                                      const QDate& currentDate) {
            domain::SsaPageRequest request;
            request.visibleColumns = {"numero_ssa", "setor_executor", "semana_executada",
                                      "responsavel_execucao"};
            request.quickSector = state.quickSector().trimmed().toStdString();
            request.excludeScaSesSte = state.excludeScaSesSte();
            request.columnFilters = state.columnFilters();
            request.advancedFilters = state.advancedFilters();
            // Narrow the scan to the current month at the database level.
            const auto weekRange = executionWeekRangeForCurrentMonth(currentDate);
            request.advancedFilters.executionWeekStart = weekRange.start;
            request.advancedFilters.executionWeekEnd = weekRange.end;
            return request;
        }
    } // namespace

    AdvancedMacroFilterViewModel::AdvancedMacroFilterViewModel(
        filterpanel::FilterPanelAdvancedState& advancedState,
        const filterpanel::FilterPanelState& filterState,
        std::shared_ptr<query::SsaQueryService> queryService, QObject* parent)
        : QObject(parent), advancedState_(advancedState), filterState_(filterState),
          reportService_(
              std::make_unique<application::SsaExecutadasReportService>(std::move(queryService))),
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

        const auto currentDate = QDate::currentDate();
        const auto request = reportRequestFromState(filterState_, currentDate);
        const auto result = reportService_->buildExecutadasReport(request, byDivision);
        if (!result.ok) {
            reportText_ = QString::fromStdString(result.error);
            return;
        }
        reportRows_.reserve(static_cast<int>(result.rows.size()));
        for (const auto& row : result.rows) {
            reportRows_.push_back(reportRowMap(row));
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
