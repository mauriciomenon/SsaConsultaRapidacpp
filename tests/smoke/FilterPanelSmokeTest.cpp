#include "domain/SsaTypes.h"
#include "presentation/AdvancedMacroFilterViewModel.h"
#include "presentation/AdvancedSectorHierarchyViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelColumnValueOptions.h"
#include "presentation/FilterPanelDistinctValueRequestBuilder.h"
#include "presentation/FilterPanelSectorViewModel.h"
#include "presentation/FilterPanelState.h"
#include "presentation/FilterPanelViewModel.h"

#include <QObject>
#include <QtTest>

#include <QDate>

#include <charconv>
#include <chrono>
#include <climits>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

    [[nodiscard]] std::string currentYearWeek() {
        int isoYear = 0;
        const int isoWeek = QDate::currentDate().weekNumber(&isoYear);
        return QString("%1%2").arg(isoYear).arg(isoWeek, 2, 10, QChar('0')).toStdString();
    }

    [[nodiscard]] ssa::domain::SsaRecord reportRecord(const std::string& ssaNumber,
                                                      const std::string& sector,
                                                      const std::string& week,
                                                      const std::string& responsible) {
        return ssa::domain::SsaRecord{std::map<std::string, std::string>{
            {"numero_ssa", ssaNumber},
            {"setor_executor", sector},
            {"semana_executada", week},
            {"responsavel_execucao", responsible},
        }};
    }

    class FilterPanelRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit FilterPanelRepository(std::chrono::milliseconds distinctDelay = {})
            : distinctDelay_(distinctDelay) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber&) const override {
            return std::nullopt;
        }
        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&) const override {
            return {};
        }

        std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest& request) const override {
            if (distinctDelay_.count() > 0) {
                std::this_thread::sleep_for(distinctDelay_);
            }
            {
                const std::scoped_lock lock(mutex_);
                distinctRequests_.push_back(request);
            }
            if (request.columnKey == "setor_executor") {
                return {"MEG2", "MAM2", "OUO7"};
            }
            if (request.columnKey == "responsavel_execucao") {
                return {"ANA", "BRUNO"};
            }
            if (request.columnKey == "num_reprogramacoes") {
                return {"0", "1", "3"};
            }
            return {};
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest& request,
                                          ssa::ports::SsaRecordConsumer consume) const override {
            const auto week = currentYearWeek();
            const std::vector<ssa::domain::SsaRecord> rows{
                reportRecord("202600001", "MEG2", week, "ANA"),
                reportRecord("202600001", "MEG2", week, "ANA"),
                reportRecord("202600002", "MEG2", week, "ANA"),
                reportRecord("202600003", "MAM2", week, "BRUNO"),
                reportRecord("202500001", "MAM2", "202501", "BRUNO"),
            };
            // Simulate the SQL semana_executada BETWEEN filter that a real
            // repository applies from advancedFilters.executionWeekStart/End.
            const auto weekStart = request.advancedFilters.executionWeekStart.value_or(0);
            const auto weekEnd = request.advancedFilters.executionWeekEnd.value_or(INT_MAX);
            std::size_t rowCount = 0;
            for (const auto& row : rows) {
                const auto rowWeek = row.valueOf("semana_executada");
                int value = 0;
                const auto parsed =
                    std::from_chars(rowWeek.data(), rowWeek.data() + rowWeek.size(), value);
                if (parsed.ec != std::errc{} || value < weekStart || value > weekEnd) {
                    continue;
                }
                if (auto error = consume(row); error.has_value()) {
                    return {rowCount, *error};
                }
                ++rowCount;
            }
            return {rowCount, {}};
        }

        [[nodiscard]] std::vector<ssa::domain::DistinctValuesRequest> distinctRequests() const {
            const std::scoped_lock lock(mutex_);
            return distinctRequests_;
        }

      private:
        std::chrono::milliseconds distinctDelay_;
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::DistinctValuesRequest> distinctRequests_;
    };

    class FilterPanelSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void quick_sector_options_are_loaded_from_distinct_executor_values() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            QTRY_VERIFY_WITH_TIMEOUT(filters.quickSectorOptions().contains("MEG2"), 1000);
            QVERIFY(filters.quickSectorOptions().contains("MAM2"));
            QTRY_VERIFY_WITH_TIMEOUT(!repository->distinctRequests().empty(), 1000);
            const auto distinctRequests = repository->distinctRequests();
            QVERIFY(std::ranges::any_of(distinctRequests, [](const auto& request) {
                return request.columnKey == "setor_executor" &&
                       request.filter.excludeScaSesSte == false;
            }));
        }

        void advanced_value_options_load_for_requested_column_without_changing_column_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("situacao");
            filters.refreshColumnValueOptionsFor("setor_executor");

            QCOMPARE(filters.columnValueOptionsLoadingFor("setor_executor"), true);
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      1000);
            QVERIFY(filters.columnValueOptionsFor("setor_executor").contains("MEG2"));
            QCOMPARE(filters.columnKey(), QString("situacao"));
        }

        void advanced_value_options_preload_queues_visible_card_values() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.preloadAdvancedColumnValueOptions();

            QCOMPARE(filters.columnValueOptionsLoadingFor("setor_executor"), true);
            QTRY_VERIFY_WITH_TIMEOUT(
                filters.columnValueOptionsFor("setor_executor").contains("MEG2"), 10000);
            QCOMPARE(filters.columnValueOptionsLoadingFor("setor_executor"), false);
            QVERIFY(filters.columnValueOptionsFor("setor_executor").contains("MEG2"));
            QTRY_VERIFY_WITH_TIMEOUT(
                filters.columnValueOptionsFor("num_reprogramacoes").contains("1"), 10000);
            QCOMPARE(filters.columnValueOptionsLoadingFor("num_reprogramacoes"), false);
            QVERIFY(filters.columnValueOptionsFor("num_reprogramacoes").contains("1"));
        }

        void column_value_options_reset_hides_stale_cache_after_filter_change() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            QSignalSpy resetSpy(&filters,
                                &ssa::presentation::FilterPanelViewModel::columnValueOptionsReset);

            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      1000);
            QVERIFY(filters.columnValueOptionsFor("setor_executor").contains("MEG2"));
            QVERIFY(
                filters.columnValuePreviewOptionsFor("setor_executor", 2, false).contains("MEG2"));
            QCOMPARE(filters.hasMoreColumnValueOptionsFor("setor_executor", 1), true);

            filters.setColumnValue("APV");

            QCOMPARE(resetSpy.count(), 1);
            QCOMPARE(filters.columnValueOptionsFor("setor_executor"), QStringList{});
            QCOMPARE(filters.columnValuePreviewOptionsFor("setor_executor", 1, false),
                     QStringList{});
            QCOMPARE(filters.hasMoreColumnValueOptionsFor("setor_executor", 1), false);

            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      3000);
            QVERIFY(filters.columnValueOptionsFor("setor_executor").contains("MEG2"));
        }

        void stale_inflight_column_value_options_are_dropped_after_filter_change() {
            auto repository =
                std::make_shared<FilterPanelRepository>(std::chrono::milliseconds{120});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.refreshColumnValueOptionsFor("setor_executor");
            QCOMPARE(filters.columnValueOptionsLoadingFor("setor_executor"), true);

            filters.setColumnValue("APV");

            QCOMPARE(filters.columnValueOptionsFor("setor_executor"), QStringList{});
            QVERIFY(
                !filters.columnValuePreviewOptionsFor("setor_executor", 1, false).contains("MEG2"));
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      3000);

            filters.preloadAdvancedColumnValueOptions();
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      3000);
            QVERIFY(filters.columnValueOptionsFor("setor_executor").contains("MEG2"));
        }

        void responsible_value_options_request_display_ordering() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.refreshColumnValueOptionsFor("responsavel_execucao");

            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("responsavel_execucao"),
                                      false, 1000);
            const auto distinctRequests = repository->distinctRequests();
            QVERIFY(std::ranges::any_of(distinctRequests, [](const auto& request) {
                return request.columnKey == "responsavel_execucao" && !request.orderByFrequency;
            }));
        }

        void column_value_options_display_priority_then_alphabetical_values() {
            ssa::presentation::FilterPanelColumnValueOptions options;

            options.store({"MEL3", "ANA", "IEE2", "IEE3", "MEL1", "BRUNO", "IEE4", "IEE1", "MEG2"},
                          QStringLiteral("setor_executor"), 1);

            QCOMPARE(options.optionsFor("setor_executor"),
                     QStringList(
                         {"IEE3", "IEE1", "IEE2", "IEE4", "MEL1", "MEL3", "ANA", "BRUNO", "MEG2"}));
        }

        void advanced_text_rows_cover_expanded_filter_fields() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            QStringList keys;
            const auto rows = text->rows();
            std::ranges::transform(rows, std::back_inserter(keys), [](const QVariant& row) {
                return row.toMap().value("key").toString();
            });

            QVERIFY(keys.size() >= 18);
            QVERIFY(keys.contains("setor_emissor"));
            QVERIFY(keys.contains("setor_executor"));
            QVERIFY(keys.contains("responsavel_execucao"));
            QVERIFY(keys.contains("status_execucao_prazo"));
            QVERIFY(keys.contains("situacao_da_parcial"));
        }

        void advanced_week_validation_accepts_empty_and_rejects_out_of_range_values() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            QVERIFY(week != nullptr);

            QCOMPARE(week->isYearValid(""), true);
            QCOMPARE(week->isYearValid("1900"), true);
            QCOMPARE(week->isYearValid("2999"), true);
            QCOMPARE(week->isYearValid("1899"), false);
            QCOMPARE(week->isYearValid("3000"), false);
            QCOMPARE(week->isWeekValid("1"), true);
            QCOMPARE(week->isWeekValid("53"), true);
            QCOMPARE(week->isWeekValid("0"), false);
            QCOMPARE(week->isWeekValid("54"), false);
            QCOMPARE(week->isYearWeekValid("202601"), true);
            QCOMPARE(week->isYearWeekValid("202653"), true);
            QCOMPARE(week->isYearWeekValid("202600"), false);
            QCOMPARE(week->isYearWeekValid("202654"), false);
        }

        void advanced_sector_hierarchy_updates_executor_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* hierarchy = qobject_cast<ssa::presentation::AdvancedSectorHierarchyViewModel*>(
                advanced->sectorHierarchy());
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(hierarchy != nullptr);
            QVERIFY(text != nullptr);

            hierarchy->applyDivision("SMIN");

            QCOMPARE(hierarchy->selectedDivision(), QString("SMIN"));
            QCOMPARE(text->textFilter("setor_executor"), QString("=IEE1,=IEE2,=IEE3,=IEE4"));
        }

        void advanced_macro_baixar_updates_status_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* macro =
                qobject_cast<ssa::presentation::AdvancedMacroFilterViewModel*>(advanced->macro());
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(macro != nullptr);
            QVERIFY(text != nullptr);

            macro->setSelectedMacro("ssas_para_baixar");

            QCOMPARE(text->textFilter("situacao"), QString("!SAD,!SCA,!SES,!STE"));
        }

        void advanced_macro_report_counts_current_month_ssas_by_sector_week_and_person() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* macro =
                qobject_cast<ssa::presentation::AdvancedMacroFilterViewModel*>(advanced->macro());
            QVERIFY(macro != nullptr);

            macro->setSelectedMacro("ssas_executadas_setor");

            QCOMPARE(macro->reportTitle(), QString("SSA Executadas Setor"));
            QCOMPARE(macro->reportRows().size(), 2);
            const auto first = macro->reportRows().at(0).toMap();
            QCOMPARE(first.value("group").toString(), QString("MAM2"));
            QCOMPARE(first.value("person").toString(), QString("BRUNO"));
            QCOMPARE(first.value("count").toInt(), 1);
            const auto second = macro->reportRows().at(1).toMap();
            QCOMPARE(second.value("group").toString(), QString("MEG2"));
            QCOMPARE(second.value("person").toString(), QString("ANA"));
            QCOMPARE(second.value("count").toInt(), 2);
        }

        void quick_sector_distinct_request_ignores_column_filters() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::FilterPanelDistinctValueRequestBuilder builder;

            state.addColumnFilter("situacao", "APV");

            const auto request = builder.quickSectorRequest(state);

            QCOMPARE(QString::fromStdString(request.columnKey), QString("setor_executor"));
            QVERIFY(request.filter.columnTerms.empty());
            QCOMPARE(request.filter.excludeScaSesSte, false);
        }

        void sector_submodel_owns_sector_ui_state() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(sector != nullptr);

            sector->setQuickSector("MEG2");
            sector->setExcludeScaSesSte(false);

            QCOMPARE(filters.quickSector(), QString("MEG2"));
            QCOMPARE(sector->excludeScaSesSte(), false);
            QCOMPARE(sector->quickSector(), QString("MEG2"));
            QCOMPARE(sector->excludeScaSesSte(), false);
        }

        void filter_summary_entries_are_structured_and_removable() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setQuickSector("MEG2");
            filters.setColumnFilters({{"situacao", "=APV"}});
            text->setTextFilter("setor_executor", "=MAM2");

            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 3, 1000);
            const auto entries = filters.activeFilterEntries();
            QCOMPARE(entries.size(), 3);
            QCOMPARE(entries.at(0).toMap().value("kind").toString(), QString("quick_sector"));
            QCOMPARE(entries.at(1).toMap().value("kind").toString(), QString("column"));
            QCOMPARE(entries.at(1).toMap().value("key").toString(), QString("situacao"));
            QCOMPARE(entries.at(2).toMap().value("kind").toString(), QString("advanced_text"));
            QCOMPARE(entries.at(2).toMap().value("key").toString(), QString("setor_executor"));

            QCOMPARE(filters.removeActiveFilter(
                         QVariantMap{{QStringLiteral("kind"), QStringLiteral("column")},
                                     {QStringLiteral("key"), QStringLiteral("situacao")}}),
                     true);

            QVERIFY(!filters.columnFilters().contains("situacao"));
            QCOMPARE(text->textFilter("setor_executor"), QString("=MAM2"));
            QCOMPARE(filters.quickSector(), QString("MEG2"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(FilterPanelSmokeTest)

#include "FilterPanelSmokeTest.moc"
