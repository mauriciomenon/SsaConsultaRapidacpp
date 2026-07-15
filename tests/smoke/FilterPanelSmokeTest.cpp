#include "AdvancedTextFilterTestSupport.h"

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "ports/IExecutadasReportPort.h"
#include "presentation/AdvancedMacroFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelColumnValueOptions.h"
#include "presentation/FilterPanelDistinctValueFetcher.h"
#include "presentation/FilterPanelDistinctValueRequestBuilder.h"
#include "presentation/FilterPanelSectorViewModel.h"
#include "presentation/FilterPanelState.h"
#include "presentation/FilterPanelViewModel.h"
#include "query/SsaQueryService.h"

#include <QObject>
#include <QtTest>

#include <QDate>
#include <QScopeGuard>

#include <charconv>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
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

    class FilterPanelRepository final : public ssa::ports::ISsaRepository,
                                        public ssa::ports::IExecutadasReportPort {
      public:
        explicit FilterPanelRepository(std::chrono::milliseconds distinctDelay = {})
            : distinctDelay_(distinctDelay) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token = {}) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return std::nullopt;
        }
        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return {};
        }

        std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest& request,
                       const std::stop_token stopToken = {}) const override {
            if (distinctDelay_.count() > 0) {
                std::this_thread::sleep_for(distinctDelay_);
            }
            std::string statusFilter;
            if (const auto status = request.filter.columnTerms.find("situacao");
                status != request.filter.columnTerms.end() && !status->second.empty()) {
                statusFilter = status->second.front().text;
            }
            {
                std::unique_lock lock(mutex_);
                distinctRequests_.push_back(request);
                if (blockStateC_ && statusFilter == "SCA" &&
                    request.columnKey == "setor_executor") {
                    stateCRequestBlocked_ = true;
                    stateCCondition_.notify_all();
                    while (!releaseStateC_ && !stopToken.stop_requested()) {
                        stateCCondition_.wait_for(lock, std::chrono::milliseconds{5});
                    }
                    stateCStopObserved_ = stopToken.stop_requested();
                    stateCRequestCompleted_ = true;
                }
            }
            if (request.columnKey == "setor_executor") {
                if (statusFilter == "SCA") {
                    return {"STATE_C_ONLY"};
                }
                if (statusFilter == "STE") {
                    return {"STATE_B_ONLY"};
                }
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

        [[nodiscard]] std::size_t maxValueLength(std::string_view,
                                                 std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest& request,
                                          ssa::ports::SsaRecordConsumer consume,
                                          std::stop_token = {}) const override {
            {
                const std::scoped_lock lock(mutex_);
                pageRequests_.push_back(request);
            }
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

        std::vector<ssa::domain::SsaExecutadasReportRow>
        executadasReport(const ssa::domain::SsaPageRequest& request, const bool byDivision,
                         const std::stop_token stopToken = {}) const override {
            std::map<std::tuple<std::string, std::string, std::string>, std::set<std::string>>
                grouped;
            const auto result = readAll(
                request,
                [&](const ssa::domain::SsaRecord& record) {
                    const auto sector = std::string{record.valueOf("setor_executor")};
                    const auto week = std::string{record.valueOf("semana_executada")};
                    const auto personValue = std::string{record.valueOf("responsavel_execucao")};
                    const auto person = personValue.empty() ? std::string{"-"} : personValue;
                    if (sector.empty() || week.empty() || record.valueOf("numero_ssa").empty()) {
                        return std::optional<std::string>{};
                    }
                    const auto group = byDivision ? sector.substr(0, 3) : sector;
                    grouped[{group, week, person}].insert(
                        std::string{record.valueOf("numero_ssa")});
                    return std::optional<std::string>{};
                },
                stopToken);
            if (!result.ok()) {
                throw std::runtime_error(result.error);
            }
            std::vector<ssa::domain::SsaExecutadasReportRow> rows;
            rows.reserve(grouped.size());
            for (const auto& [key, numbers] : grouped) {
                rows.push_back({std::get<0>(key), std::get<1>(key), std::get<2>(key),
                                static_cast<int>(numbers.size())});
            }
            return rows;
        }

        [[nodiscard]] std::vector<ssa::domain::DistinctValuesRequest> distinctRequests() const {
            const std::scoped_lock lock(mutex_);
            return distinctRequests_;
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> pageRequests() const {
            const std::scoped_lock lock(mutex_);
            return pageRequests_;
        }

        void blockStateCRequest() {
            const std::scoped_lock lock(mutex_);
            blockStateC_ = true;
            releaseStateC_ = false;
            stateCRequestBlocked_ = false;
            stateCRequestCompleted_ = false;
            stateCStopObserved_ = false;
        }

        void releaseStateCRequest() {
            const std::scoped_lock lock(mutex_);
            releaseStateC_ = true;
            stateCCondition_.notify_all();
        }

        [[nodiscard]] bool stateCRequestBlocked() const {
            const std::scoped_lock lock(mutex_);
            return stateCRequestBlocked_;
        }

        [[nodiscard]] bool stateCRequestCompleted() const {
            const std::scoped_lock lock(mutex_);
            return stateCRequestCompleted_;
        }

        [[nodiscard]] bool stateCStopObserved() const {
            const std::scoped_lock lock(mutex_);
            return stateCStopObserved_;
        }

      private:
        std::chrono::milliseconds distinctDelay_;
        mutable std::mutex mutex_;
        mutable std::condition_variable stateCCondition_;
        mutable std::vector<ssa::domain::DistinctValuesRequest> distinctRequests_;
        mutable std::vector<ssa::domain::SsaPageRequest> pageRequests_;
        bool blockStateC_{false};
        mutable bool releaseStateC_{false};
        mutable bool stateCRequestBlocked_{false};
        mutable bool stateCRequestCompleted_{false};
        mutable bool stateCStopObserved_{false};
    };

    class FilterPanelSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void column_filter_rows_start_with_catalog_labels_and_empty_values() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::ColumnFilterViewModel columns(state);

            const auto rows = columns.rows();
            QVERIFY(!rows.isEmpty());

            const auto first = rows.front().toMap();
            const auto key = first.value("key").toString().toStdString();
            const auto* column = ssa::domain::ColumnCatalog::find(key);
            QVERIFY(column != nullptr);
            QCOMPARE(first.value("label").toString(), QString::fromStdString(column->label));
            QCOMPARE(first.value("value").toString(), QString{});
        }

        void excluded_status_label_comes_from_domain_policy() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            QCOMPARE(filters.excludedStatusCodesText(), QString("SCA/SES/STE"));
        }

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

        void advanced_value_options_wait_for_explicit_request() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            QTest::qWait(400);
            const auto hasAdvancedRequest = [repository] {
                const auto requests = repository->distinctRequests();
                return std::ranges::any_of(requests, [](const auto& request) {
                    return request.limit == ssa::domain::kAdvancedDistinctValuesLimit;
                });
            };
            QVERIFY(!hasAdvancedRequest());

            filters.setColumnValue("APV");
            QTest::qWait(400);
            QVERIFY(!hasAdvancedRequest());

            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      1000);
            QVERIFY(hasAdvancedRequest());
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

            filters.setColumnValue("APV");

            QCOMPARE(resetSpy.count(), 1);
            QCOMPARE(filters.columnValueOptionsFor("setor_executor"), QStringList{});

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
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      3000);

            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      3000);
            QVERIFY(filters.columnValueOptionsFor("setor_executor").contains("MEG2"));
        }

        void undo_state_replacement_drops_blocked_distinct_result() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(sector != nullptr);
            repository->blockStateCRequest();
            auto releaseBlockedRequest =
                qScopeGuard([repository] { repository->releaseStateCRequest(); });

            ssa::ports::UserPreferencesSnapshot stateC;
            stateC.filters.columnFilters = {{"situacao", "=SCA"}};
            filters.applyPreferences(stateC);
            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCRequestBlocked(), 1000);
            QSignalSpy resetSpy(&filters,
                                &ssa::presentation::FilterPanelViewModel::columnValueOptionsReset);

            auto stateB = stateC;
            stateB.filters.columnFilters = {{"situacao", "=STE"}};
            bool staleRequestVisibleDuringReplacement = false;
            connect(sector, &ssa::presentation::FilterPanelSectorViewModel::changed, &filters,
                    [&filters, &staleRequestVisibleDuringReplacement] {
                        const auto columnFilters = filters.columnFilters();
                        if (const auto status = columnFilters.find("situacao");
                            status != columnFilters.end() && status->second == "=STE") {
                            staleRequestVisibleDuringReplacement =
                                staleRequestVisibleDuringReplacement ||
                                filters.columnValueOptionsLoadingFor("setor_executor");
                        }
                    });
            filters.applyPreferences(stateB);
            QVERIFY(!staleRequestVisibleDuringReplacement);
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCStopObserved(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCRequestCompleted(), 1000);
            QCOMPARE(resetSpy.count(), 1);
            QTest::qWait(50);

            QCOMPARE(filters.columnValueOptionsFor("setor_executor"), QStringList{});
            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("setor_executor"), false,
                                      1000);
            QCOMPARE(filters.columnValueOptionsFor("setor_executor"), QStringList{"STATE_B_ONLY"});
        }

        void set_column_filters_stops_distinct_before_publishing_new_state_once() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(sector != nullptr);
            filters.setColumnFilters({{"situacao", "=SCA"}});
            repository->blockStateCRequest();
            auto releaseBlockedRequest =
                qScopeGuard([repository] { repository->releaseStateCRequest(); });
            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCRequestBlocked(), 1000);
            QSignalSpy resetSpy(&filters,
                                &ssa::presentation::FilterPanelViewModel::columnValueOptionsReset);
            bool staleLoadingWasPublished = false;
            connect(sector, &ssa::presentation::FilterPanelSectorViewModel::changed, &filters,
                    [&filters, &staleLoadingWasPublished] {
                        const auto columnFilters = filters.columnFilters();
                        if (const auto status = columnFilters.find("situacao");
                            status != columnFilters.end() && status->second == "=STE") {
                            staleLoadingWasPublished =
                                staleLoadingWasPublished ||
                                filters.columnValueOptionsLoadingFor("setor_executor");
                        }
                    });

            filters.setColumnFilters({{"situacao", "=STE"}});

            QVERIFY(!staleLoadingWasPublished);
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCStopObserved(), 1000);
            QCOMPARE(resetSpy.count(), 1);
        }

        void reset_filters_stops_distinct_before_publishing_empty_state_once() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(sector != nullptr);
            filters.setColumnFilters({{"situacao", "=SCA"}});
            repository->blockStateCRequest();
            auto releaseBlockedRequest =
                qScopeGuard([repository] { repository->releaseStateCRequest(); });
            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCRequestBlocked(), 1000);
            QSignalSpy resetSpy(&filters,
                                &ssa::presentation::FilterPanelViewModel::columnValueOptionsReset);
            bool staleLoadingWasPublished = false;
            connect(sector, &ssa::presentation::FilterPanelSectorViewModel::changed, &filters,
                    [&filters, &staleLoadingWasPublished] {
                        if (filters.columnFilters().empty()) {
                            staleLoadingWasPublished =
                                staleLoadingWasPublished ||
                                filters.columnValueOptionsLoadingFor("setor_executor");
                        }
                    });

            filters.resetFilters();

            QVERIFY(!staleLoadingWasPublished);
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCStopObserved(), 1000);
            QCOMPARE(resetSpy.count(), 1);
        }

        void nested_state_replacement_keeps_invalidation_scoped() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(sector != nullptr);
            ssa::ports::UserPreferencesSnapshot stateA;
            stateA.filters.columnFilters = {{"situacao", "=SCA"}};
            filters.applyPreferences(stateA);
            repository->blockStateCRequest();
            auto releaseBlockedRequest =
                qScopeGuard([repository] { repository->releaseStateCRequest(); });
            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCRequestBlocked(), 1000);
            QSignalSpy resetSpy(&filters,
                                &ssa::presentation::FilterPanelViewModel::columnValueOptionsReset);
            bool nestedReplacementStarted = false;
            bool staleLoadingWasPublished = false;
            connect(sector, &ssa::presentation::FilterPanelSectorViewModel::changed, &filters,
                    [&filters, &nestedReplacementStarted, &staleLoadingWasPublished] {
                        const auto columnFilters = filters.columnFilters();
                        const auto status = columnFilters.find("situacao");
                        if (status == columnFilters.end()) {
                            return;
                        }
                        if (status->second == "=STE" && !nestedReplacementStarted) {
                            nestedReplacementStarted = true;
                            filters.setColumnFilters({{"situacao", "=APV"}});
                        }
                        if (status->second == "=STE" || status->second == "=APV") {
                            staleLoadingWasPublished =
                                staleLoadingWasPublished ||
                                filters.columnValueOptionsLoadingFor("setor_executor");
                        }
                    });
            auto stateB = stateA;
            stateB.filters.columnFilters = {{"situacao", "=STE"}};

            filters.applyPreferences(stateB);

            QVERIFY(nestedReplacementStarted);
            QVERIFY(!staleLoadingWasPublished);
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCStopObserved(), 1000);
            QCOMPARE(filters.columnFilters().at("situacao"), std::string{"=APV"});
            QCOMPARE(resetSpy.count(), 2);
        }

        void superseded_distinct_request_stops_active_and_publishes_only_latest() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelDistinctValueFetcher fetcher(service);
            repository->blockStateCRequest();
            auto releaseBlockedRequest =
                qScopeGuard([repository] { repository->releaseStateCRequest(); });

            std::vector<std::uint64_t> publishedTokens;
            std::vector<std::vector<std::string>> publishedValues;
            connect(&fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesReady,
                    &fetcher,
                    [&publishedTokens, &publishedValues](const std::uint64_t requestToken,
                                                         std::vector<std::string> values) {
                        publishedTokens.push_back(requestToken);
                        publishedValues.push_back(std::move(values));
                    });

            ssa::domain::DistinctValuesRequest requestA;
            requestA.columnKey = "setor_executor";
            requestA.filter.columnTerms["situacao"] = {
                {"SCA", ssa::domain::MatchMode::Equals, false}};
            fetcher.requestValues(requestA, 1, false);
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCRequestBlocked(), 1000);

            auto requestB = requestA;
            requestB.filter.columnTerms["situacao"] = {
                {"STE", ssa::domain::MatchMode::Equals, false}};
            fetcher.requestValues(requestB, 2, false);

            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCStopObserved(), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(publishedTokens.size(), std::size_t{1}, 1000);
            QCOMPARE(publishedTokens.front(), std::uint64_t{2});
            QCOMPARE(publishedValues.front(), std::vector<std::string>{"STATE_B_ONLY"});
        }

        void canceled_distinct_request_clears_its_column_loading() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            filters.setColumnFilters({{"situacao", "=SCA"}});
            repository->blockStateCRequest();
            auto releaseBlockedRequest =
                qScopeGuard([repository] { repository->releaseStateCRequest(); });

            filters.refreshColumnValueOptionsFor("setor_executor");
            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCRequestBlocked(), 1000);
            filters.refreshColumnValueOptionsFor("responsavel_execucao");

            QTRY_VERIFY_WITH_TIMEOUT(repository->stateCStopObserved(), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(filters.columnValueOptionsLoadingFor("responsavel_execucao"),
                                      false, 1000);
            QCOMPARE(filters.columnValueOptionsFor("responsavel_execucao"),
                     (QStringList{"ANA", "BRUNO"}));
            QCOMPARE(filters.columnValueOptionsLoadingFor("setor_executor"), false);
        }

        void exclude_sca_ses_ste_change_invalidates_once_and_applies_once() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            QSignalSpy resetSpy(&filters,
                                &ssa::presentation::FilterPanelViewModel::columnValueOptionsReset);
            QSignalSpy applySpy(&filters, &ssa::presentation::FilterPanelViewModel::applyRequested);

            filters.setExcludeScaSesSte(true);

            QCOMPARE(resetSpy.count(), 1);
            QCOMPARE(applySpy.count(), 1);
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
                return request.columnKey == "responsavel_execucao" && !request.orderByFrequency &&
                       request.limit == ssa::domain::kAdvancedDistinctValuesLimit;
            }));
        }

        void quick_sector_request_keeps_small_limit() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::FilterPanelDistinctValueRequestBuilder builder;

            const auto request = builder.quickSectorRequest(state);

            QCOMPARE(QString::fromStdString(request.columnKey), QString("setor_executor"));
            QCOMPARE(request.limit, std::size_t{500});
        }

        void column_value_options_preserve_repository_display_order() {
            ssa::presentation::FilterPanelColumnValueOptions options;

            options.store({"MEL3", "ANA", "IEE2", "IEE3", "MEL1", "BRUNO", "IEE4", "IEE1", "MEG2"},
                          QStringLiteral("setor_executor"), 1);

            QCOMPARE(options.optionsFor("setor_executor"),
                     QStringList(
                         {"MEL3", "ANA", "IEE2", "IEE3", "MEL1", "BRUNO", "IEE4", "IEE1", "MEG2"}));
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

            const auto keys = ssa::tests::advancedTextFilterKeys(*text);

            QCOMPARE(keys.size(), 11);
            QVERIFY(keys.contains("setor_emissor"));
            QVERIFY(keys.contains("setor_executor"));
            QVERIFY(keys.contains("responsavel_execucao"));
            QVERIFY(keys.contains("status_execucao_prazo"));
            QVERIFY(keys.contains("situacao_da_parcial"));
            QVERIFY(!keys.contains("grau_prioridade_emissao"));
            QVERIFY(!keys.contains("grau_prioridade_planejamento"));
            QVERIFY(!keys.contains("execucao_simples"));
            QVERIFY(!keys.contains("equipamento"));
            QVERIFY(!keys.contains("servico_origem"));
            QVERIFY(!keys.contains("sistema_origem"));
            QVERIFY(!keys.contains("justificativa"));
            QVERIFY(!keys.contains("parciais"));
            QVERIFY(!keys.contains("execucao_parcial"));
        }

        void advanced_text_filter_state_syncs_for_all_text_filter_rows() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* columns =
                qobject_cast<ssa::presentation::ColumnFilterViewModel*>(filters.columns());
            QVERIFY(text != nullptr);
            QVERIFY(columns != nullptr);

            for (const auto& key : ssa::tests::advancedTextFilterKeys(*text)) {
                QVERIFY2(!key.isEmpty(), "advanced text row key must not be empty");

                filters.setColumnFilters({{key.toStdString(), "OLD"}});
                text->replaceWithOperatorValueLists(key, {"IN"}, {"OUT"});

                QVERIFY2(!filters.columnFilters().contains(key.toStdString()),
                         qPrintable(QString("column filter was not removed for %1").arg(key)));
                QCOMPARE(text->textFilter(key), QString("=IN,!OUT"));
                QCOMPARE(text->operatorModeFor(key), QString("mixed"));
                QCOMPARE(ssa::tests::advancedTextFilterCardState(*text, key)
                             .value("textFilter")
                             .toString(),
                         QString("=IN,!OUT"));

                QVERIFY2(columns->applyFilterFor(key, "NEW"),
                         qPrintable(QString("column filter apply failed for %1").arg(key)));
                QCOMPARE(text->textFilter(key), QString(""));
                QVERIFY2(filters.columnFilters().contains(key.toStdString()),
                         qPrintable(QString("column filter was not applied for %1").arg(key)));
            }
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

        void advanced_macro_default_option_is_graph_only() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* macro =
                qobject_cast<ssa::presentation::AdvancedMacroFilterViewModel*>(advanced->macro());
            QVERIFY(macro != nullptr);

            const auto options = macro->options();
            QVERIFY(!options.isEmpty());
            QCOMPARE(options.first().toMap().value("label").toString(),
                     QString("Exibir o grafico e somente o grafico"));
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
            QTRY_COMPARE_WITH_TIMEOUT(macro->reportRows().size(), 2, 1000);
            QVERIFY(!macro->reportLoading());
            const auto first = macro->reportRows().at(0).toMap();
            QCOMPARE(first.value("group").toString(), QString("MAM2"));
            QCOMPARE(first.value("person").toString(), QString("BRUNO"));
            QCOMPARE(first.value("count").toInt(), 1);
            const auto second = macro->reportRows().at(1).toMap();
            QCOMPARE(second.value("group").toString(), QString("MEG2"));
            QCOMPARE(second.value("person").toString(), QString("ANA"));
            QCOMPARE(second.value("count").toInt(), 2);
        }

        void advanced_macro_report_preserves_iso_weeks_across_year_boundaries() {
            const auto verifyRange = [](const QDate& date, const int expectedStart,
                                        const int expectedEnd) {
                auto repository = std::make_shared<FilterPanelRepository>();
                auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
                ssa::presentation::filterpanel::FilterPanelState state;
                ssa::presentation::AdvancedMacroFilterViewModel macro(
                    state.advanced(), state, service, nullptr, [date] { return date; });

                macro.setSelectedMacro("ssas_executadas_setor");

                QTRY_COMPARE_WITH_TIMEOUT(repository->pageRequests().size(), std::size_t{1}, 1000);
                const auto request = repository->pageRequests().front();
                QCOMPARE(request.advancedFilters.executionWeekStart,
                         std::optional<int>{expectedStart});
                QCOMPARE(request.advancedFilters.executionWeekEnd, std::optional<int>{expectedEnd});
            };

            verifyRange(QDate(2021, 1, 15), 202053, 202104);
            verifyRange(QDate(2018, 12, 15), 201848, 201901);
        }

        void advanced_macro_request_uses_canonical_executor_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* macro =
                qobject_cast<ssa::presentation::AdvancedMacroFilterViewModel*>(advanced->macro());
            QVERIFY(macro != nullptr);

            filters.setQuickSector("MEG2");
            macro->setSelectedMacro("ssas_executadas_setor");

            QTRY_COMPARE_WITH_TIMEOUT(repository->pageRequests().size(), std::size_t{1}, 1000);
            const auto requests = repository->pageRequests();
            QCOMPARE(requests.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(requests.front().quickSector), QString(""));
            QVERIFY(requests.front().advancedFilters.textFilters.contains("setor_executor"));
            QCOMPARE(QString::fromStdString(
                         requests.front().advancedFilters.textFilters.at("setor_executor")),
                     QString("=MEG2"));
        }

        void quick_sector_is_visible_in_executor_advanced_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setQuickSector("IEE3");

            QTRY_COMPARE_WITH_TIMEOUT(
                ssa::tests::advancedTextFilterCardState(*text, "setor_executor")
                    .value("textFilter")
                    .toString(),
                QString("=IEE3"), 1000);
            QCOMPARE(text->textFilter("setor_executor"), QString("=IEE3"));
            QCOMPARE(filters.quickSector(), QString(""));
        }

        void executor_advanced_edit_folds_quick_sector_into_single_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setQuickSector("IEE3");

            QVERIFY(text->updateFilterWithSelectedValue("setor_executor", "IEE1"));

            QTRY_COMPARE_WITH_TIMEOUT(filters.quickSector(), QString(""), 1000);
            QCOMPARE(text->textFilter("setor_executor"), QString("=IEE3,=IEE1"));
            QCOMPARE(ssa::tests::advancedTextFilterCardState(*text, "setor_executor")
                         .value("textFilter")
                         .toString(),
                     QString("=IEE3,=IEE1"));
            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 1, 1000);
            const auto entry = filters.activeFilterEntries().at(0).toMap();
            QCOMPARE(entry.value("kind").toString(), QString("advanced_text"));
            QCOMPARE(entry.value("key").toString(), QString("setor_executor"));
        }

        void clearing_executor_advanced_filter_also_clears_quick_sector() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setQuickSector("IEE3");

            QVERIFY(text->clearTextFilterAndApply("setor_executor"));

            QTRY_COMPARE_WITH_TIMEOUT(filters.quickSector(), QString(""), 1000);
            QCOMPARE(text->textFilter("setor_executor"), QString(""));
            QCOMPARE(ssa::tests::advancedTextFilterCardState(*text, "setor_executor")
                         .value("textFilter")
                         .toString(),
                     QString(""));
        }

        void quick_sector_distinct_request_ignores_only_executor_filters() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::FilterPanelDistinctValueRequestBuilder builder;

            state.addColumnFilter("situacao", "APV");
            state.addColumnFilter("setor_executor", "IEE3");
            state.advanced().setTextFilter("setor_executor", "=IEE3,!IEE1");

            const auto request = builder.quickSectorRequest(state);

            QCOMPARE(QString::fromStdString(request.columnKey), QString("setor_executor"));
            QVERIFY(request.filter.columnTerms.contains("situacao"));
            QVERIFY(!request.filter.columnTerms.contains("setor_executor"));
            QCOMPARE(request.filter.excludeScaSesSte, false);
            QVERIFY(!request.filter.advanced.textFilters.contains("setor_executor"));
            QVERIFY(request.filter.advanced.textFilters.empty());
        }

        void column_value_request_ignores_filters_for_requested_column() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::FilterPanelDistinctValueRequestBuilder builder;

            state.setQuickSector("IEE3");
            state.setExcludeScaSesSte(true);
            state.addColumnFilter("setor_executor", "=IEE3");
            state.addColumnFilter("situacao", "=APV");
            state.advanced().setTextFilter("setor_executor", "=IEE3,!IEE1");
            state.advanced().setTextFilter("responsavel_execucao", "=ANA");

            const auto request = builder.columnValuesRequestFor(state, "setor_executor");

            QVERIFY(request.has_value());
            QCOMPARE(QString::fromStdString(request->columnKey), QString("setor_executor"));
            QVERIFY(!request->filter.quickSector.has_value());
            QCOMPARE(request->filter.excludeScaSesSte, true);
            QVERIFY(!request->filter.columnTerms.contains("setor_executor"));
            QVERIFY(request->filter.columnTerms.contains("situacao"));
            QVERIFY(!request->filter.advanced.textFilters.contains("setor_executor"));
            QCOMPARE(QString::fromStdString(
                         request->filter.advanced.textFilters.at("responsavel_execucao")),
                     QString("=ANA"));
        }

        void status_value_request_keeps_status_values_available_when_exclusion_is_active() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::FilterPanelDistinctValueRequestBuilder builder;

            state.setExcludeScaSesSte(true);
            state.addColumnFilter("situacao", "=APV");
            state.advanced().setTextFilter("situacao", "!STE");
            state.advanced().setTextFilter("setor_executor", "=IEE3");

            const auto request = builder.columnValuesRequestFor(state, "situacao");

            QVERIFY(request.has_value());
            QCOMPARE(request->filter.excludeScaSesSte, false);
            QVERIFY(!request->filter.columnTerms.contains("situacao"));
            QVERIFY(!request->filter.advanced.textFilters.contains("situacao"));
            QCOMPARE(
                QString::fromStdString(request->filter.advanced.textFilters.at("setor_executor")),
                QString("=IEE3"));
        }

        void column_value_request_ignores_non_text_advanced_filters_for_requested_column() {
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::FilterPanelDistinctValueRequestBuilder builder;

            state.advanced().setExecutionYear("2026");
            state.advanced().setExecutionWeekStart("202601");
            state.advanced().setExecutionWeekEnd("202620");
            state.advanced().setIssueYear("2025");
            state.advanced().setReprogrammingValues("1,3");
            state.advanced().setOnlyReprogrammed(true);
            state.advanced().setDerivationMode("derived");

            const auto executionWeekRequest =
                builder.columnValuesRequestFor(state, "semana_executada");
            QVERIFY(executionWeekRequest.has_value());
            QVERIFY(!executionWeekRequest->filter.advanced.executionYear.has_value());
            QVERIFY(!executionWeekRequest->filter.advanced.executionWeekStart.has_value());
            QVERIFY(!executionWeekRequest->filter.advanced.executionWeekEnd.has_value());
            QVERIFY(executionWeekRequest->filter.advanced.issueYear.has_value());
            QCOMPARE(executionWeekRequest->filter.advanced.onlyReprogrammed, false);

            const auto reprogrammingRequest =
                builder.columnValuesRequestFor(state, "num_reprogramacoes");
            QVERIFY(reprogrammingRequest.has_value());
            QVERIFY(reprogrammingRequest->filter.advanced.reprogrammingValues.empty());
            QCOMPARE(reprogrammingRequest->filter.advanced.onlyReprogrammed, false);
            QVERIFY(reprogrammingRequest->filter.advanced.executionYear.has_value());

            const auto derivationRequest = builder.columnValuesRequestFor(state, "derivada_de");
            QVERIFY(derivationRequest.has_value());
            QCOMPARE(derivationRequest->filter.advanced.derivationMode,
                     ssa::domain::DerivationFilterMode::All);
            QVERIFY(derivationRequest->filter.advanced.executionYear.has_value());
        }

        void advanced_week_state_rejects_invalid_year_and_week_inputs() {
            ssa::presentation::filterpanel::FilterPanelAdvancedState state;
            ssa::presentation::AdvancedWeekFilterViewModel week(
                state, {"semana_programada", "semana_cadastro", "semana_executada"});
            QSignalSpy changedSpy(&week, &ssa::presentation::AdvancedWeekFilterViewModel::changed);

            week.setIssueYearFilter("2");
            week.setExecutionYearFilter("3000");
            week.setWeekFilter("54");

            QCOMPARE(week.issueYearFilter(), QString());
            QCOMPARE(week.executionYearFilter(), QString());
            QCOMPARE(week.weekFilter(), QString());
            QCOMPARE(changedSpy.count(), 0);
            QVERIFY(!state.filters().issueYear.has_value());
            QVERIFY(!state.filters().executionYear.has_value());
            QVERIFY(!state.filters().week.has_value());

            week.setIssueYearFilter("1900");
            week.setExecutionYearFilter("2999");
            week.setWeekFilter("53");

            QCOMPARE(state.filters().issueYear, std::optional<int>{1900});
            QCOMPARE(state.filters().executionYear, std::optional<int>{2999});
            QCOMPARE(state.filters().week, std::optional<int>{53});
            QCOMPARE(changedSpy.count(), 3);
        }

        void advanced_week_state_publishes_only_complete_year_week_ranges() {
            ssa::presentation::filterpanel::FilterPanelAdvancedState state;
            ssa::presentation::AdvancedWeekFilterViewModel week(
                state, {"semana_programada", "semana_cadastro", "semana_executada"});
            QSignalSpy changedSpy(&week, &ssa::presentation::AdvancedWeekFilterViewModel::changed);

            week.setIssueWeekStartFilter("20261");
            week.setIssueWeekEndFilter("202600");
            week.setExecutionWeekStartFilter("189953");
            week.setExecutionWeekEndFilter("300001");

            QCOMPARE(week.issueWeekStartFilter(), QString());
            QCOMPARE(week.issueWeekEndFilter(), QString());
            QCOMPARE(week.executionWeekStartFilter(), QString());
            QCOMPARE(week.executionWeekEndFilter(), QString());
            QCOMPARE(changedSpy.count(), 0);

            week.setIssueWeekStartFilter("190001");
            week.setIssueWeekEndFilter("202653");
            week.setExecutionWeekStartFilter("202601");
            week.setExecutionWeekEndFilter("299953");

            const auto filters = state.filters();
            QCOMPARE(filters.issueWeekStart, std::optional<int>{190001});
            QCOMPARE(filters.issueWeekEnd, std::optional<int>{202653});
            QCOMPARE(filters.executionWeekStart, std::optional<int>{202601});
            QCOMPARE(filters.executionWeekEnd, std::optional<int>{299953});
            QCOMPARE(changedSpy.count(), 4);
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

            QCOMPARE(filters.quickSector(), QString(""));
            QCOMPARE(sector->excludeScaSesSte(), false);
            QCOMPARE(sector->quickSector(), QString("MEG2"));
            QCOMPARE(sector->excludeScaSesSte(), false);
        }

        void sector_selector_reflects_single_executor_advanced_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);
            QVERIFY(sector != nullptr);

            text->setTextFilter("setor_executor", "=IEE3");

            QTRY_COMPARE_WITH_TIMEOUT(sector->quickSector(), QString("IEE3"), 1000);
            QCOMPARE(filters.quickSector(), QString(""));
            QVERIFY(sector->selectorIndex() > 0);

            sector->setQuickSector("");

            QCOMPARE(text->textFilter("setor_executor"), QString(""));
            QCOMPARE(sector->quickSector(), QString(""));
            QCOMPARE(filters.quickSector(), QString(""));
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

            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 2, 1000);
            const auto entries = filters.activeFilterEntries();
            QCOMPARE(entries.size(), 2);
            QVariantMap executorEntry;
            QVariantMap statusEntry;
            for (const auto& entryValue : entries) {
                const auto entry = entryValue.toMap();
                if (entry.value("kind").toString() == "advanced_text" &&
                    entry.value("key").toString() == "setor_executor") {
                    executorEntry = entry;
                } else if (entry.value("kind").toString() == "column" &&
                           entry.value("key").toString() == "situacao") {
                    statusEntry = entry;
                }
            }
            QVERIFY(!executorEntry.isEmpty());
            QVERIFY(!statusEntry.isEmpty());
            QCOMPARE(text->textFilter("setor_executor"), QString("=MAM2"));

            QCOMPARE(filters.removeActiveFilter(
                         QVariantMap{{QStringLiteral("kind"), QStringLiteral("column")},
                                     {QStringLiteral("key"), QStringLiteral("situacao")}}),
                     true);

            QVERIFY(!filters.columnFilters().contains("situacao"));
            QCOMPARE(text->textFilter("setor_executor"), QString("=MAM2"));
            QCOMPARE(filters.quickSector(), QString(""));

            QCOMPARE(filters.removeActiveFilter(executorEntry), true);
            QCOMPARE(text->textFilter("setor_executor"), QString(""));
            QCOMPARE(filters.quickSector(), QString(""));
        }

        void executor_shortcut_writes_advanced_filter_and_single_chip() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);
            QVERIFY(sector != nullptr);

            filters.setQuickSector("IEE3");

            QTRY_COMPARE_WITH_TIMEOUT(text->textFilter("setor_executor"), QString("=IEE3"), 1000);
            QCOMPARE(filters.quickSector(), QString(""));
            QCOMPARE(sector->quickSector(), QString("IEE3"));
            QCOMPARE(ssa::tests::advancedTextFilterCardState(*text, "setor_executor")
                         .value("textFilter")
                         .toString(),
                     QString("=IEE3"));
            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 1, 1000);
            const auto entry = filters.activeFilterEntries().at(0).toMap();
            QCOMPARE(entry.value("kind").toString(), QString("advanced_text"));
            QCOMPARE(entry.value("key").toString(), QString("setor_executor"));
        }

        void executor_shortcut_replaces_simple_shortcut_value() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);
            QVERIFY(sector != nullptr);

            filters.setQuickSector("IEE3");
            filters.setQuickSector("IEE1");

            QTRY_COMPARE_WITH_TIMEOUT(text->textFilter("setor_executor"), QString("=IEE1"), 1000);
            QCOMPARE(filters.quickSector(), QString(""));
            QCOMPARE(sector->quickSector(), QString("IEE1"));
            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 1, 1000);
        }

        void executor_shortcut_clear_preserves_mixed_manual_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);
            QVERIFY(sector != nullptr);

            text->setTextFilter("setor_executor", "=IEE1,!IEE3");
            sector->setQuickSector("");

            QCOMPARE(text->textFilter("setor_executor"), QString("=IEE1,!IEE3"));
            QCOMPARE(sector->quickSector(), QString(""));
            QCOMPARE(filters.quickSector(), QString(""));
            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 1, 1000);
        }

        void executor_chip_removal_clears_advanced_filter_and_selector() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);
            QVERIFY(sector != nullptr);

            filters.setQuickSector("IEE3");
            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 1, 1000);

            QCOMPARE(filters.removeActiveFilter(filters.activeFilterEntries().at(0).toMap()), true);

            QCOMPARE(text->textFilter("setor_executor"), QString(""));
            QCOMPARE(sector->quickSector(), QString(""));
            QCOMPARE(filters.quickSector(), QString(""));
        }

        void advanced_text_filter_removes_matching_column_filter_for_any_column() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setColumnFilters({{"responsavel_execucao", "ANA"}, {"situacao", "=APV"}});
            text->setTextFilter("responsavel_execucao", "=BRUNO");

            QTRY_VERIFY_WITH_TIMEOUT(!filters.columnFilters().contains("responsavel_execucao"),
                                     1000);
            QVERIFY(filters.columnFilters().contains("situacao"));
            QCOMPARE(text->textFilter("responsavel_execucao"), QString("=BRUNO"));
            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 2, 1000);
            QVERIFY(!ssa::tests::advancedTextFilterCardState(*text, "responsavel_execucao")
                         .value("textFilter")
                         .toString()
                         .isEmpty());
        }

        void column_filter_removes_matching_advanced_text_filter_for_any_column() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* columns =
                qobject_cast<ssa::presentation::ColumnFilterViewModel*>(filters.columns());
            QVERIFY(text != nullptr);
            QVERIFY(columns != nullptr);

            text->setTextFilter("solicitante", "=ANA");
            QVERIFY(columns->applyFilterFor("solicitante", "BRUNO"));

            QCOMPARE(text->textFilter("solicitante"), QString(""));
            QVERIFY(filters.columnFilters().contains("solicitante"));
            QCOMPARE(QString::fromStdString(filters.columnFilters().at("solicitante")),
                     QString("BRUNO"));
            QTRY_COMPARE_WITH_TIMEOUT(filters.activeFilterEntries().size(), 1, 1000);
            QCOMPARE(filters.activeFilterEntries().at(0).toMap().value("kind").toString(),
                     QString("column"));
        }

        void status_shortcut_reflects_and_canonicalizes_column_status_filter() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setColumnFilters({{"situacao", "=APV"}});

            QVERIFY(filters.statusShortcutSelected("APV"));
            filters.toggleStatusShortcut("STE");

            QTRY_VERIFY_WITH_TIMEOUT(!filters.columnFilters().contains("situacao"), 1000);
            QCOMPARE(text->textFilter("situacao"), QString("=APV,=STE"));
            QVERIFY(filters.statusShortcutSelected("APV"));
            QVERIFY(filters.statusShortcutSelected("STE"));
        }

        void clear_status_shortcuts_removes_column_and_advanced_status_filters() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            filters.setColumnFilters({{"situacao", "=APV"}});
            text->setTextFilter("situacao", "=STE");

            filters.clearStatusShortcuts();

            QCOMPARE(text->textFilter("situacao"), QString(""));
            QVERIFY(!filters.columnFilters().contains("situacao"));
            QVERIFY(!filters.statusShortcutSelected("APV"));
            QVERIFY(!filters.statusShortcutSelected("STE"));
        }

        void status_shortcut_from_existing_exclusion_disables_code() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setTextFilter("situacao", "!STE");

            filters.toggleStatusShortcut("STE");

            QCOMPARE(text->textFilter("situacao"), QString(""));
            QVERIFY(!filters.statusShortcutSelected("STE"));
        }

        void clear_status_shortcuts_removes_shortcut_exclusions() {
            auto repository = std::make_shared<FilterPanelRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setTextFilter("situacao", "=APV,!STE");

            QVERIFY(filters.statusShortcutSelected("APV"));
            QVERIFY(!filters.statusShortcutSelected("STE"));

            filters.clearStatusShortcuts();

            QCOMPARE(text->textFilter("situacao"), QString(""));
            QVERIFY(!filters.columnFilters().contains("situacao"));
            QVERIFY(!filters.statusShortcutSelected("APV"));
            QVERIFY(!filters.statusShortcutSelected("STE"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(FilterPanelSmokeTest)

#include "FilterPanelSmokeTest.moc"
