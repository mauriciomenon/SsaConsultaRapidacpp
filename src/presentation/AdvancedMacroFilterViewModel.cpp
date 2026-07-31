#include "presentation/AdvancedMacroFilterViewModel.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "presentation/AsyncOperationErrorLog.h"

#include <QDate>
#include <QVariantMap>
#include <QtConcurrentRun>

#include <algorithm>
#include <map>
#include <set>
#include <system_error>
#include <utility>

namespace ssa::presentation {

    namespace {
        constexpr auto kNone = "";
        constexpr auto kGraph = "ssas_executadas_grafico";
        constexpr auto kBaixar = "ssas_para_baixar";
        constexpr auto kExecutadasSetor = "ssas_executadas_setor";
        constexpr auto kExecutadasDivisao = "ssas_executadas_divisao";

        // Range of ISO year-weeks (YYYYWW) overlapping the month of currentDate.
        // The database restricts the set via semana_executada BETWEEN, so the
        // macro report never loads the full table just to filter it in memory.
        struct YearWeekRange {
            int start{0};
            int end{0};
        };

        YearWeekRange executionWeekRangeForCurrentMonth(const QDate& currentDate) {
            const QDate firstOfMonth(currentDate.year(), currentDate.month(), 1);
            const QDate lastOfMonth(currentDate.year(), currentDate.month(),
                                    currentDate.daysInMonth());
            int startIsoYear = 0;
            const int startWeek = firstOfMonth.weekNumber(&startIsoYear);
            int endIsoYear = 0;
            const int endWeek = lastOfMonth.weekNumber(&endIsoYear);
            const int first = startIsoYear * domain::kYearWeekMultiplier + startWeek;
            const int last = endIsoYear * domain::kYearWeekMultiplier + endWeek;
            return {(std::min)(first, last), (std::max)(first, last)};
        }

        QVariantMap reportRowMap(const application::ExecutadasReportRow& row) {
            return QVariantMap{{"group", QString::fromStdString(row.group)},
                               {"week", QString::fromStdString(row.week)},
                               {"person", QString::fromStdString(row.person)},
                               {"count", row.count}};
        }

        QString chartWeek(const std::string& value) {
            const QString week = QString::fromStdString(value);
            return week.size() == 6 ? week.left(4) + QStringLiteral("-W") + week.mid(4) : week;
        }

        QVariantMap reportChartMap(const std::vector<application::ExecutadasReportRow>& rows) {
            std::set<QString> categories;
            std::set<QString> groups;
            for (const auto& row : rows) {
                categories.insert(chartWeek(row.week));
                groups.insert(QString::fromStdString(row.group));
            }

            const bool includeGroup = groups.size() > 1;
            std::map<QString, std::map<QString, int>> counts;
            for (const auto& row : rows) {
                const QString group = QString::fromStdString(row.group);
                const QString person = QString::fromStdString(row.person);
                const QString name = includeGroup ? group + QStringLiteral(" / ") + person : person;
                counts[name][chartWeek(row.week)] += row.count;
            }

            QStringList categoryList;
            for (const auto& category : categories) {
                categoryList.push_back(category);
            }
            QVariantList series;
            for (const auto& [name, valuesByCategory] : counts) {
                QVariantList values;
                int total = 0;
                for (const auto& category : categoryList) {
                    const auto found = valuesByCategory.find(category);
                    const int value = found == valuesByCategory.end() ? 0 : found->second;
                    values.push_back(value);
                    total += value;
                }
                series.push_back(QVariantMap{{QStringLiteral("name"), name},
                                             {QStringLiteral("values"), values},
                                             {QStringLiteral("trendValues"), QVariantList{}},
                                             {QStringLiteral("total"), total}});
            }
            return {{QStringLiteral("categories"), categoryList},
                    {QStringLiteral("series"), series},
                    {QStringLiteral("subtitle"), QObject::tr("Mes atual")},
                    {QStringLiteral("qualityText"), QString{}},
                    {QStringLiteral("available"), true}};
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
            request.pageSize = 0;
            return request;
        }
    } // namespace

    AdvancedMacroFilterViewModel::AdvancedMacroFilterViewModel(
        filterpanel::FilterPanelAdvancedState& advancedState,
        const filterpanel::FilterPanelState& filterState,
        std::shared_ptr<ports::IExecutadasReportPort> reportPort, QObject* parent,
        CurrentDate currentDate)
        : QObject(parent), advancedState_(advancedState), filterState_(filterState),
          reportService_(
              std::make_shared<application::SsaExecutadasReportService>(std::move(reportPort))),
          currentDate_(std::move(currentDate)),
          options_{
              QVariantMap{{"label", tr("Exibir o grafico e somente o grafico")},
                          {"value", QString::fromLatin1(kGraph)}},
              QVariantMap{{"label", tr("Baixar")}, {"value", QString::fromLatin1(kBaixar)}},
              QVariantMap{{"label", tr("SSA Executadas Setor")},
                          {"value", QString::fromLatin1(kExecutadasSetor)}},
              QVariantMap{{"label", tr("SSA Executadas Divisao")},
                          {"value", QString::fromLatin1(kExecutadasDivisao)}},
          } {}

    AdvancedMacroFilterViewModel::~AdvancedMacroFilterViewModel() {
        shuttingDown_ = true;
        for (const auto& operation : reportOperations_) {
            disconnect(&operation->watcher, nullptr, this, nullptr);
        }
        stopReportOperations();
    }

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
        emit changed();
        if (selectedMacro_ == QString::fromLatin1(kBaixar)) {
            stopReportOperations();
            applyBaixarPreset();
            clearReport();
            emit filterStateChanged();
            emit reportChanged();
        } else if (selectedMacro_ == QString::fromLatin1(kGraph)) {
            buildExecutadasReport(QString::fromLatin1(kExecutadasSetor), true);
            selectedMacro_.clear();
            emit changed();
        } else if (selectedMacro_ == QString::fromLatin1(kExecutadasSetor) ||
                   selectedMacro_ == QString::fromLatin1(kExecutadasDivisao)) {
            buildExecutadasReport(selectedMacro_);
            selectedMacro_.clear();
            emit changed();
        } else {
            stopReportOperations();
            latestReportOperationId_ = 0;
            clearReport();
            emit reportChanged();
        }
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

    const QVariantMap& AdvancedMacroFilterViewModel::reportChart() const {
        return reportChart_;
    }

    bool AdvancedMacroFilterViewModel::reportGraphOnly() const {
        return reportGraphOnly_;
    }

    bool AdvancedMacroFilterViewModel::reportLoading() const {
        return reportLoading_;
    }

    QString AdvancedMacroFilterViewModel::reportError() const {
        return reportError_;
    }

    bool AdvancedMacroFilterViewModel::hasActiveOperations() const {
        return std::ranges::any_of(reportOperations_,
                                   [](const auto& operation) { return !operation->completed; });
    }

    void AdvancedMacroFilterViewModel::cancel() {
        stopReportOperations();
    }

    void AdvancedMacroFilterViewModel::refreshFromState() {}

    void AdvancedMacroFilterViewModel::applyBaixarPreset() {
        advancedState_.setTextFilter(
            QString::fromStdString(std::string{domain::ColumnCatalog::statusColumnKey()}),
            QString::fromStdString(
                std::string{domain::ColumnCatalog::downloadableStatusFilterExpression()}));
    }

    void AdvancedMacroFilterViewModel::buildExecutadasReport(const QString& value,
                                                             const bool graphOnly) {
        const bool byDivision = value == QString::fromLatin1(kExecutadasDivisao);
        stopReportOperations();
        reportTitle_ = graphOnly    ? tr("SSA Executadas por Setor")
                       : byDivision ? tr("SSA Executadas Divisao")
                                    : tr("SSA Executadas Setor");
        reportText_.clear();
        reportRows_.clear();
        reportChart_.clear();
        reportError_.clear();
        reportLoading_ = true;
        reportGraphOnly_ = graphOnly;

        const auto currentDate = currentDate_();
        auto request = reportRequestFromState(filterState_, currentDate);
        auto operation = std::make_unique<ReportOperation>();
        operation->id = ++nextReportOperationId_;
        operation->byDivision = byDivision;
        operation->state = std::make_shared<ReportTaskState>();
        const auto operationId = operation->id;
        const auto service = reportService_;
        const auto state = operation->state;
        const auto stopToken = operation->stopSource.get_token();
        connect(&operation->watcher, &QFutureWatcher<void>::finished, this,
                [this, operationId] { finishExecutadasReport(operationId); });
        auto* watcher = &operation->watcher;
        latestReportOperationId_ = operationId;
        reportOperations_.push_back(std::move(operation));
        emit activeOperationsChanged();
        emit reportChanged();

        watcher->setFuture(QtConcurrent::run(
            [service, request = std::move(request), byDivision, stopToken, state] {
                try {
                    auto result = service->buildExecutadasReport(request, byDivision, stopToken);
                    std::scoped_lock lock(state->mutex);
                    state->result = std::move(result);
                    state->canceled = stopToken.stop_requested();
                } catch (const std::system_error& error) {
                    std::scoped_lock lock(state->mutex);
                    if (error.code() == std::make_error_code(std::errc::operation_canceled)) {
                        state->canceled = true;
                    } else {
                        state->error = std::current_exception();
                    }
                } catch (...) {
                    std::scoped_lock lock(state->mutex);
                    state->error = std::current_exception();
                }
            }));
    }

    void AdvancedMacroFilterViewModel::finishExecutadasReport(const std::uint64_t operationId) {
        const auto found =
            std::ranges::find_if(reportOperations_, [operationId](const auto& operation) {
                return operation->id == operationId;
            });
        if (found == reportOperations_.end()) {
            return;
        }
        auto& operation = **found;
        operation.completed = true;
        emit activeOperationsChanged();
        if (!shuttingDown_ && operation.id == latestReportOperationId_) {
            std::optional<application::ExecutadasReportResult> result = std::nullopt;
            std::exception_ptr error;
            bool canceled = false;
            {
                std::scoped_lock lock(operation.state->mutex);
                result = std::move(operation.state->result);
                error = operation.state->error;
                canceled = operation.state->canceled || operation.stopSource.stop_requested();
            }
            reportLoading_ = false;
            reportError_.clear();
            if (canceled) {
                logAsyncOperationError("Macro report failed after cancellation:", error);
            } else if (error) {
                try {
                    std::rethrow_exception(error);
                } catch (const std::exception& exception) {
                    reportError_ = QString::fromUtf8(exception.what());
                } catch (...) {
                    reportError_ = tr("Falha interna ao gerar relatorio");
                }
            } else if (!canceled && result) {
                if (!result->ok) {
                    reportError_ = QString::fromStdString(result->error);
                } else {
                    reportChart_ = reportChartMap(result->rows);
                    reportRows_.reserve(static_cast<int>(result->rows.size()));
                    for (const auto& row : result->rows) {
                        reportRows_.push_back(reportRowMap(row));
                    }
                }
            }
            if (!canceled) {
                if (!reportError_.isEmpty()) {
                    reportText_ = reportError_;
                } else {
                    reportText_ =
                        reportRows_.empty()
                            ? tr("Nenhuma SSA baixada no mes vigente para o recorte atual.")
                            : tr("%1 linhas agrupadas no mes vigente.").arg(reportRows_.size());
                }
            }
            emit reportChanged();
        }
        QMetaObject::invokeMethod(this, [this] { pruneCompletedReports(); }, Qt::QueuedConnection);
    }

    void AdvancedMacroFilterViewModel::stopReportOperations() {
        for (const auto& operation : reportOperations_) {
            if (!operation->completed) {
                operation->stopSource.request_stop();
                operation->watcher.cancel();
            }
        }
    }

    void AdvancedMacroFilterViewModel::pruneCompletedReports() {
        std::erase_if(reportOperations_,
                      [](const auto& operation) { return operation->completed; });
    }

    void AdvancedMacroFilterViewModel::clearReport() {
        reportTitle_.clear();
        reportText_.clear();
        reportRows_.clear();
        reportChart_.clear();
        reportError_.clear();
        reportLoading_ = false;
        reportGraphOnly_ = false;
    }

} // namespace ssa::presentation
