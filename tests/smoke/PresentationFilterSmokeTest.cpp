#include "AdvancedTextFilterTestSupport.h"
#include "PresentationSmokeFakes.h"

#include "ports/IExecutadasReportPort.h"
#include "ports/OperationError.h"
#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedMacroFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/ExportViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelDistinctValueFetcher.h"
#include "presentation/FilterPanelDistinctValueRequestBuilder.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/FilterPreferencesNormalizer.h"
#include "presentation/MainViewModel.h"
#include "presentation/PageQueryCoordinator.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableModel.h"
#include "presentation/WorkflowCommandRunner.h"
#include "query/SsaQueryService.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QThreadPool>
#include <QVariantMap>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>

namespace {

    using ssa::tests::presentation_smoke::activeFilterEntry;
    using ssa::tests::presentation_smoke::CapturingImportPort;
    using ssa::tests::presentation_smoke::FakeCommands;
    using ssa::tests::presentation_smoke::FakeFilterPresetStore;
    using ssa::tests::presentation_smoke::FakePreferences;
    using ssa::tests::presentation_smoke::FakeRepository;

    class BlockingPageRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit BlockingPageRepository(const bool blockDistinct = false,
                                        const bool failDistinct = false,
                                        const bool failAfterDistinctCancel = false)
            : blockDistinct_(blockDistinct), failDistinct_(failDistinct),
              failAfterDistinctCancel_(failAfterDistinctCancel) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest&,
                                        const std::stop_token stopToken = {}) const override {
            started_.store(true, std::memory_order_release);
            while (!stopToken.stop_requested()) {
                std::this_thread::yield();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            finished_.store(true, std::memory_order_release);
            throw std::system_error(std::make_error_code(std::errc::operation_canceled));
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
        distinctValues(const ssa::domain::DistinctValuesRequest&,
                       const std::stop_token stopToken = {}) const override {
            distinctCalls_.fetch_add(1, std::memory_order_relaxed);
            {
                const std::scoped_lock lock(metricsMutex_);
                distinctThread_ = std::this_thread::get_id();
            }
            metricsStarted_.store(true, std::memory_order_release);
            if (blockDistinct_) {
                while (!stopToken.stop_requested()) {
                    std::this_thread::yield();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
                metricsFinished_.store(true, std::memory_order_release);
                if (failAfterDistinctCancel_) {
                    throw std::runtime_error("distinct failed after cancel");
                }
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            metricsFinished_.store(true, std::memory_order_release);
            if (failDistinct_) {
                throw std::runtime_error("distinct failed");
            }
            return {"A", "Longest"};
        }

        [[nodiscard]] std::size_t maxValueLength(std::string_view,
                                                 std::stop_token = {}) const override {
            const std::scoped_lock lock(metricsMutex_);
            maxLengthThread_ = std::this_thread::get_id();
            return 12;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] bool started() const {
            return started_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool finished() const {
            return finished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool metricsStarted() const {
            return metricsStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool metricsFinished() const {
            return metricsFinished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] int distinctCalls() const {
            return distinctCalls_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] bool metricsShareWorkerThread() const {
            const std::scoped_lock lock(metricsMutex_);
            return distinctThread_ != std::thread::id{} && distinctThread_ == maxLengthThread_;
        }

      private:
        bool blockDistinct_{false};
        bool failDistinct_{false};
        bool failAfterDistinctCancel_{false};
        mutable std::atomic_bool started_{false};
        mutable std::atomic_bool finished_{false};
        mutable std::atomic_bool metricsStarted_{false};
        mutable std::atomic_bool metricsFinished_{false};
        mutable std::atomic_int distinctCalls_{0};
        mutable std::mutex metricsMutex_;
        mutable std::thread::id distinctThread_;
        mutable std::thread::id maxLengthThread_;
    };

    class FailSecondPageRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit FailSecondPageRepository(const bool genericFailure = false)
            : genericFailure_(genericFailure) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            const std::scoped_lock lock(mutex_);
            requests_.push_back(request);
            if (requests_.size() == 2) {
                if (genericFailure_) {
                    throw std::runtime_error("filter B path=/tmp/private.sqlite rc=11");
                }
                throw ssa::ports::OperationError("Falha ao acessar o banco de dados",
                                                 "filter B failed rc=11");
            }
            const auto description = request.searchText.empty() ? "Inicial" : request.searchText;
            return {{ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                             {"situacao", "APV"},
                                             {"descricao_ssa", description}}}},
                    1,
                    request.pageIndex,
                    request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token = {}) const override {
            return 1;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return std::nullopt;
        }

        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber&, std::stop_token = {}) const override {
            return {};
        }

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token = {}) const override {
            return {};
        }

        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

      private:
        bool genericFailure_{false};
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::SsaPageRequest> requests_;
    };

    class MutableDistinctRepository final : public ssa::ports::ISsaRepository {
      public:
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

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest& request,
                                                std::stop_token = {}) const override {
            const std::scoped_lock lock(mutex_);
            if (request.columnKey == "setor_executor" &&
                request.limit == ssa::domain::kDefaultDistinctValuesLimit) {
                ++quickSectorRequests_;
                if (failedQuickSectorRequestsRemaining_ > 0) {
                    --failedQuickSectorRequestsRemaining_;
                    throw std::runtime_error("quick sector failed once");
                }
                return {"MEG2", "MAM2"};
            }
            if (request.limit != ssa::presentation::kAdvancedDistinctValuesLimit ||
                request.columnKey != "situacao") {
                return {};
            }
            ++advancedRequests_;
            if (failedRequestsRemaining_ > 0) {
                --failedRequestsRemaining_;
                throw std::runtime_error("distinct failed once");
            }
            return values_;
        }

        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            const std::scoped_lock lock(mutex_);
            std::size_t maximum = 0;
            for (const auto& value : values_) {
                maximum = (std::max)(maximum, value.size());
            }
            return maximum;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
        }

        void failNextAdvancedRequest() {
            const std::scoped_lock lock(mutex_);
            ++failedRequestsRemaining_;
        }

        void failNextQuickSectorRequest() {
            const std::scoped_lock lock(mutex_);
            ++failedQuickSectorRequestsRemaining_;
        }

        void setValues(std::vector<std::string> values) {
            const std::scoped_lock lock(mutex_);
            values_ = std::move(values);
        }

        [[nodiscard]] int advancedRequests() const {
            const std::scoped_lock lock(mutex_);
            return advancedRequests_;
        }

        [[nodiscard]] int quickSectorRequests() const {
            const std::scoped_lock lock(mutex_);
            return quickSectorRequests_;
        }

      private:
        mutable std::mutex mutex_;
        mutable std::vector<std::string> values_{"A", "Longest"};
        mutable int failedRequestsRemaining_{0};
        mutable int advancedRequests_{0};
        mutable int failedQuickSectorRequestsRemaining_{0};
        mutable int quickSectorRequests_{0};
    };

    class BlockingMacroReportRepository final : public ssa::ports::ISsaRepository,
                                                public ssa::ports::IExecutadasReportPort {
      public:
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

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token = {}) const override {
            return {};
        }

        std::size_t maxValueLength(std::string_view, std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest& request,
                                          ssa::ports::SsaRecordConsumer consume,
                                          const std::stop_token stopToken = {}) const override {
            const int call = calls_.fetch_add(1, std::memory_order_acq_rel);
            {
                const std::scoped_lock lock(mutex_);
                requests_.push_back(request);
            }
            if (call == 0) {
                firstStarted_.store(true, std::memory_order_release);
                const auto deadline =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds{500};
                while (!stopToken.stop_requested() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::yield();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{200});
                firstFinished_.store(true, std::memory_order_release);
                if (stopToken.stop_requested()) {
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
                }
            } else {
                secondStarted_.store(true, std::memory_order_release);
            }
            const ssa::domain::SsaRecord record{{{"numero_ssa", "202600001"},
                                                 {"setor_executor", "MEG2"},
                                                 {"semana_executada", "202601"},
                                                 {"responsavel_execucao", "ANA"}}};
            if (const auto error = consume(record); error.has_value()) {
                return {0, *error};
            }
            return {1, {}};
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

        [[nodiscard]] bool firstStarted() const {
            return firstStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool firstFinished() const {
            return firstFinished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool secondStarted() const {
            return secondStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

      private:
        mutable std::atomic_int calls_{0};
        mutable std::atomic_bool firstStarted_{false};
        mutable std::atomic_bool firstFinished_{false};
        mutable std::atomic_bool secondStarted_{false};
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::SsaPageRequest> requests_;
    };

    class BlockingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest&,
                            std::stop_token = {}) override {
            return run();
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest&,
                                          std::stop_token = {}) override {
            return run();
        }

        [[nodiscard]] bool started() const {
            return started_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool finished() const {
            return finished_.load(std::memory_order_acquire);
        }

      private:
        ssa::ports::WorkflowResult run() {
            started_.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            finished_.store(true, std::memory_order_release);
            return {ssa::ports::WorkflowStatus::Succeeded, "finished"};
        }

        std::atomic_bool started_{false};
        std::atomic_bool finished_{false};
    };

    class ThrowingExportPort final : public ssa::ports::IExportPort {
      public:
        ssa::ports::WorkflowResult exportFilteredList(const ssa::ports::ExportFilteredListRequest&,
                                                      std::stop_token = {}) override {
            throw std::runtime_error("export failed");
        }
    };

    class CapturingExportPort final : public ssa::ports::IExportPort {
      public:
        ssa::ports::WorkflowResult
        exportFilteredList(const ssa::ports::ExportFilteredListRequest& request,
                           std::stop_token = {}) override {
            request_ = request;
            return {ssa::ports::WorkflowStatus::Succeeded, "exported"};
        }

        [[nodiscard]] const ssa::ports::ExportFilteredListRequest& request() const {
            return request_;
        }

      private:
        ssa::ports::ExportFilteredListRequest request_;
    };

    void populateTableModel(ssa::presentation::SsaTableModel& model) {
        ssa::domain::SsaPageResult page;
        page.rows.push_back(
            ssa::domain::SsaRecord{{{"numero_ssa", "202500001"}, {"situacao", "APV"}}});
        page.totalRows = 1;
        page.pageSize = 1;
        std::vector<std::string> keys{"numero_ssa"};
        auto displayColumns = ssa::presentation::SsaColumnDisplayCatalog{}.resolveAll(keys);
        ssa::presentation::SsaTableDisplayValues displayValues;
        displayValues.values.emplace_back(QStringLiteral("202500001"));
        displayValues.rowCount = 1;
        displayValues.columnCount = 1;
        model.setPage(std::move(page), std::move(keys), std::move(displayColumns),
                      std::move(displayValues));
    }

    class PresentationFilterSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void distinct_value_fetcher_queries_values_and_width_on_the_same_worker() {
            auto repository = std::make_shared<BlockingPageRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelDistinctValueFetcher fetcher(service);
            bool ready = false;
            std::vector<std::string> values;
            std::size_t maxValueLength = 0;
            connect(&fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesReady,
                    this,
                    [&](const std::uint64_t requestToken, std::vector<std::string> receivedValues,
                        const std::size_t receivedMaxValueLength) {
                        QCOMPARE(requestToken, 42);
                        values = std::move(receivedValues);
                        maxValueLength = receivedMaxValueLength;
                        ready = true;
                    });
            ssa::domain::DistinctValuesRequest request;
            request.columnKey = "situacao";

            fetcher.requestValues(request, 42, true);

            QTRY_VERIFY_WITH_TIMEOUT(ready, 1000);
            QCOMPARE(values, std::vector<std::string>({"A", "Longest"}));
            QCOMPARE(maxValueLength, 12);
            QVERIFY(repository->metricsShareWorkerThread());
        }

        void filter_panel_stores_the_measured_column_value_length() {
            auto repository = std::make_shared<BlockingPageRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filterPanel(service);

            filterPanel.refreshColumnValueOptionsFor(QStringLiteral("situacao"));

            QTRY_VERIFY_WITH_TIMEOUT(
                !filterPanel.columnValueOptionsLoadingFor(QStringLiteral("situacao")), 1000);
            QCOMPARE(filterPanel.columnValueOptionsFor(QStringLiteral("situacao")),
                     QStringList({QStringLiteral("A"), QStringLiteral("Longest")}));
            QCOMPARE(filterPanel.columnValueMaxLengthFor(QStringLiteral("situacao")), 12);
        }

        void filter_panel_retries_distinct_values_after_query_failure() {
            auto repository = std::make_shared<MutableDistinctRepository>();
            repository->failNextAdvancedRequest();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filterPanel(service);
            QTest::ignoreMessage(QtWarningMsg, "Column value query failed: distinct failed once");

            filterPanel.refreshColumnValueOptionsFor(QStringLiteral("situacao"));

            QTRY_VERIFY_WITH_TIMEOUT(
                !filterPanel.columnValueOptionsLoadingFor(QStringLiteral("situacao")), 1000);
            QCOMPARE(repository->advancedRequests(), 1);
            QVERIFY(filterPanel.columnValueOptionsFor(QStringLiteral("situacao")).isEmpty());
            QCOMPARE(filterPanel.columnValueOptionsErrorFor(QStringLiteral("situacao")),
                     QString("Falha ao consultar valores"));

            filterPanel.refreshColumnValueOptionsFor(QStringLiteral("situacao"));

            QTRY_COMPARE_WITH_TIMEOUT(repository->advancedRequests(), 2, 1000);
            const QStringList expectedValues{QStringLiteral("A"), QStringLiteral("Longest")};
            QTRY_COMPARE_WITH_TIMEOUT(filterPanel.columnValueOptionsFor(QStringLiteral("situacao")),
                                      expectedValues, 1000);
            QCOMPARE(filterPanel.columnValueOptionsErrorFor(QStringLiteral("situacao")), QString());
        }

        void quick_sector_distinct_failure_is_visible_and_retryable() {
            auto repository = std::make_shared<MutableDistinctRepository>();
            repository->failNextQuickSectorRequest();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            QTest::ignoreMessage(QtWarningMsg,
                                 "Column value query failed: quick sector failed once");
            ssa::presentation::FilterPanelViewModel filterPanel(service);
            auto* sector =
                qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filterPanel.sector());
            QVERIFY(sector != nullptr);

            QTRY_COMPARE_WITH_TIMEOUT(sector->optionsError(), QString("Falha ao consultar valores"),
                                      1000);
            QCOMPARE(repository->quickSectorRequests(), 1);

            filterPanel.retryQuickSectorOptions();

            QTRY_COMPARE_WITH_TIMEOUT(repository->quickSectorRequests(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(sector->optionsError(), QString(), 1000);
            QVERIFY(sector->selectorValues().contains(QStringLiteral("MEG2")));
        }

        void exclusion_summary_uses_filter_tokens_not_rendered_text() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filterPanel(service);

            filterPanel.setColumnFilters({{"descricao_ssa", "=Exc: literal"}});
            QCOMPARE(filterPanel.hasExclusionFilter(), false);

            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                filterPanel.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);
            text->setTextFilter(QStringLiteral("situacao"), QStringLiteral("!APV"));

            QCOMPARE(filterPanel.hasExclusionFilter(), true);
        }

        void reprogramming_values_mark_the_table_column_as_filtered() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filterPanel(service);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                filterPanel.advanced());
            QVERIFY(advanced != nullptr);
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(derivation != nullptr);

            derivation->setReprogrammingValues({QStringLiteral("2")});

            QCOMPARE(filterPanel.hasFilterForColumn(QStringLiteral("num_reprogramacoes")), true);
        }

        void macro_report_is_async_latest_wins_and_does_not_publish_filter_state() {
            auto repository = std::make_shared<BlockingMacroReportRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filterPanel(service);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                filterPanel.advanced());
            QVERIFY(advanced != nullptr);
            auto* macro =
                qobject_cast<ssa::presentation::AdvancedMacroFilterViewModel*>(advanced->macro());
            QVERIFY(macro != nullptr);
            QSignalSpy filterStateSpy(
                macro, &ssa::presentation::AdvancedMacroFilterViewModel::filterStateChanged);
            QSignalSpy reportSpy(macro,
                                 &ssa::presentation::AdvancedMacroFilterViewModel::reportChanged);
            QElapsedTimer elapsed;
            elapsed.start();

            macro->setSelectedMacro(QStringLiteral("ssas_executadas_setor"));

            QVERIFY(elapsed.elapsed() < 100);
            QVERIFY(macro->reportLoading());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            macro->setSelectedMacro(QStringLiteral("ssas_executadas_divisao"));

            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 1000);
            QVERIFY(!repository->firstFinished());
            QTRY_VERIFY_WITH_TIMEOUT(!macro->reportLoading(), 1000);
            QCOMPARE(macro->reportError(), QString());
            QCOMPARE(macro->reportTitle(), QString("SSA Executadas Divisao"));
            QCOMPARE(macro->reportRows().size(), 1);
            QCOMPARE(macro->reportRows().front().toMap().value("group").toString(), QString("MEG"));
            QCOMPARE(filterStateSpy.size(), 0);
            QVERIFY(reportSpy.size() >= 2);
            const auto requests = repository->requests();
            QCOMPARE(requests.size(), std::size_t{2});
            QCOMPARE(requests.at(0).pageSize, std::size_t{0});
            QCOMPARE(requests.at(1).pageSize, std::size_t{0});
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QCOMPARE(macro->reportTitle(), QString("SSA Executadas Divisao"));
        }

        void macro_report_destructor_is_non_blocking_and_keeps_worker_dependencies_alive() {
            auto repository = std::make_shared<BlockingMacroReportRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            QElapsedTimer destructionTimer;
            {
                ssa::presentation::filterpanel::FilterPanelState state;
                ssa::presentation::AdvancedMacroFilterViewModel macro(state.advanced(), state,
                                                                      service);
                macro.setSelectedMacro(QStringLiteral("ssas_executadas_setor"));
                QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);
                destructionTimer.start();
            }

            QVERIFY(destructionTimer.elapsed() < 50);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
        }

        void cancelAfterMacroWorkerCompletedBeforeTerminalDoesNotPublishReport() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::filterpanel::FilterPanelState state;
            ssa::presentation::AdvancedMacroFilterViewModel macro(state.advanced(), state, service);

            macro.setSelectedMacro(QStringLiteral("ssas_executadas_setor"));
            QVERIFY(QThreadPool::globalInstance()->waitForDone(1000));
            QVERIFY(macro.reportLoading());

            macro.cancel();

            QTRY_VERIFY_WITH_TIMEOUT(!macro.reportLoading(), 1000);
            QCOMPARE(macro.reportRows().size(), 0);
            QCOMPARE(macro.reportText(), QString());
            QCOMPARE(macro.reportError(), QString());
        }

        void successful_workflow_invalidates_cached_distinct_values() {
            auto repository = std::make_shared<MutableDistinctRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            auto* filters = model.browse()->filters();
            const QStringList initialValues{QStringLiteral("A"), QStringLiteral("Longest")};

            filters->refreshColumnValueOptionsFor(QStringLiteral("situacao"));
            QTRY_COMPARE_WITH_TIMEOUT(filters->columnValueOptionsFor(QStringLiteral("situacao")),
                                      initialValues, 1000);
            QCOMPARE(repository->advancedRequests(), 1);
            repository->setValues({"Updated"});

            model.actions()->workflows()->rescanIncremental();
            QTRY_COMPARE_WITH_TIMEOUT(model.actions()->workflows()->lastSucceeded(), true, 1000);
            filters->refreshColumnValueOptionsFor(QStringLiteral("situacao"));

            QTRY_COMPARE_WITH_TIMEOUT(repository->advancedRequests(), 2, 1000);
            const QStringList updatedValues{QStringLiteral("Updated")};
            QTRY_COMPARE_WITH_TIMEOUT(filters->columnValueOptionsFor(QStringLiteral("situacao")),
                                      updatedValues, 1000);
        }

        void distinct_value_fetcher_reports_real_query_failure_separately() {
            auto repository = std::make_shared<BlockingPageRepository>(false, true);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelDistinctValueFetcher fetcher(service);
            int readyCount = 0;
            bool failed = false;
            QString failureMessage;
            connect(
                &fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesReady, this,
                [&](std::uint64_t, const std::vector<std::string>&, std::size_t) { ++readyCount; });
            connect(&fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesFailed,
                    this, [&](const std::uint64_t requestToken, const QString& message) {
                        QCOMPARE(requestToken, 1);
                        failureMessage = message;
                        failed = true;
                    });
            ssa::domain::DistinctValuesRequest request;
            request.columnKey = "situacao";
            QTest::ignoreMessage(QtWarningMsg, "Column value query failed: distinct failed");

            fetcher.requestValues(request, 1, true);

            QTRY_VERIFY_WITH_TIMEOUT(failed, 1000);
            QCOMPARE(readyCount, 0);
            QCOMPARE(failureMessage, QString("Falha ao consultar valores"));
            QVERIFY(!failureMessage.contains("distinct failed"));
        }

        void distinct_value_fetcher_reports_missing_service_separately() {
            ssa::presentation::FilterPanelDistinctValueFetcher fetcher(nullptr);
            QSignalSpy failureSpy(
                &fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesFailed);
            ssa::domain::DistinctValuesRequest request;
            request.columnKey = "situacao";

            fetcher.requestValues(request, 7, false);

            QCOMPARE(failureSpy.count(), 1);
            QCOMPARE(failureSpy.at(0).at(0).toULongLong(), quint64{7});
            QCOMPARE(failureSpy.at(0).at(1).toString(),
                     QStringLiteral("browse service is not configured"));
        }

        void distinct_value_fetcher_destructor_is_non_blocking_and_drops_callback() {
            auto repository = std::make_shared<BlockingPageRepository>(true, false);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            int callbackCount = 0;
            QElapsedTimer destructionTimer;
            {
                ssa::presentation::FilterPanelDistinctValueFetcher fetcher(service);
                connect(&fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesReady,
                        this, [&](std::uint64_t, const std::vector<std::string>&, std::size_t) {
                            ++callbackCount;
                        });
                connect(&fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesFailed,
                        this, [&](std::uint64_t) { ++callbackCount; });
                ssa::domain::DistinctValuesRequest request;
                request.columnKey = "situacao";
                fetcher.requestValues(request, 1, true);
                QTRY_VERIFY_WITH_TIMEOUT(repository->metricsStarted(), 1000);
                destructionTimer.start();
            }

            QVERIFY(destructionTimer.elapsed() < 50);
            QTRY_VERIFY_WITH_TIMEOUT(repository->metricsFinished(), 1000);
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
        }

        void page_query_destructor_is_non_blocking_and_drops_callback() {
            auto repository = std::make_shared<BlockingPageRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            int callbackCount = 0;
            QElapsedTimer destructionTimer;

            {
                ssa::presentation::PageQueryCoordinator coordinator(service);
                connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                        [&callbackCount] { ++callbackCount; });
                connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, this,
                        [&callbackCount] { ++callbackCount; });
                connect(&coordinator, &ssa::presentation::PageQueryCoordinator::failed, this,
                        [&callbackCount] { ++callbackCount; });
                coordinator.run({});
                QTRY_VERIFY_WITH_TIMEOUT(repository->started(), 1000);
                destructionTimer.start();
            }

            QVERIFY(destructionTimer.elapsed() < 50);
            QTRY_VERIFY_WITH_TIMEOUT(repository->finished(), 1000);
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
        }

        void workflow_runner_destructor_is_non_blocking_and_drops_callback() {
            auto importPort = std::make_shared<BlockingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            int callbackCount = 0;
            QElapsedTimer destructionTimer;

            {
                ssa::presentation::WorkflowCommandRunner runner(workflows);
                connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                        [&callbackCount] { ++callbackCount; });
                runner.rescan(ssa::ports::RescanMode::Incremental);
                QTRY_VERIFY_WITH_TIMEOUT(importPort->started(), 1000);
                destructionTimer.start();
            }

            QVERIFY(destructionTimer.elapsed() < 50);
            QTRY_VERIFY_WITH_TIMEOUT(importPort->finished(), 1000);
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
        }

        void application_shutdown_waits_for_distinct_query_terminal() {
            auto repository = std::make_shared<BlockingPageRepository>(true, false);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->filters()->refreshColumnValueOptionsFor(QStringLiteral("situacao"));
            QTRY_VERIFY_WITH_TIMEOUT(repository->metricsStarted(), 1000);
            QVERIFY(model.canCancelActivity());

            model.requestShutdown();

            QVERIFY(!model.shutdownReady());
            QVERIFY(!model.canCancelActivity());
            QTRY_VERIFY_WITH_TIMEOUT(repository->metricsFinished(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(model.shutdownReady(), 1000);
        }

        void contextual_cancel_reports_canceling_until_distinct_terminal() {
            auto repository = std::make_shared<BlockingPageRepository>(true, false);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            model.browse()->filters()->refreshColumnValueOptionsFor(QStringLiteral("situacao"));
            QTRY_VERIFY_WITH_TIMEOUT(repository->metricsStarted(), 1000);

            model.requestCancelAll();

            QVERIFY(model.cancelingActivity());
            QVERIFY(!model.canCancelActivity());
            QVERIFY(model.browse()->filters()->columnValueOptionsLoadingFor(
                QStringLiteral("situacao")));
            QTRY_VERIFY_WITH_TIMEOUT(repository->metricsFinished(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.cancelingActivity(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.browse()->filters()->columnValueOptionsLoadingFor(
                                         QStringLiteral("situacao")),
                                     1000);
            QCOMPARE(model.browse()->status()->message(), QString("Operacao cancelada"));

            const auto callsBeforeRetry = repository->distinctCalls();
            model.browse()->filters()->refreshColumnValueOptionsFor(QStringLiteral("situacao"));
            QTRY_VERIFY_WITH_TIMEOUT(repository->distinctCalls() > callsBeforeRetry, 1000);
            QVERIFY(model.browse()->filters()->columnValueOptionsLoadingFor(
                QStringLiteral("situacao")));
            model.requestCancelAll();
            QTRY_VERIFY_WITH_TIMEOUT(!model.browse()->filters()->columnValueOptionsLoadingFor(
                                         QStringLiteral("situacao")),
                                     1000);
        }

        void distinct_cancel_logs_a_concurrent_failure_without_publishing_it() {
            QTest::ignoreMessage(QtWarningMsg,
                                 "Column value query canceled after failure: distinct failed after "
                                 "cancel");
            auto repository = std::make_shared<BlockingPageRepository>(true, false, true);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->filters()->refreshColumnValueOptionsFor(QStringLiteral("situacao"));
            QTRY_VERIFY_WITH_TIMEOUT(repository->metricsStarted(), 1000);
            model.requestCancelAll();

            QTRY_VERIFY_WITH_TIMEOUT(repository->metricsFinished(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.cancelingActivity(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.browse()->filters()->columnValueOptionsLoadingFor(
                                         QStringLiteral("situacao")),
                                     1000);
            QCOMPARE(model.browse()->status()->message(), QString("Operacao cancelada"));
        }

        void export_view_model_converts_port_exception_to_failure() {
            auto exportPort = std::make_shared<ThrowingExportPort>();
            auto workflows =
                std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
            ssa::presentation::ExportViewModel viewModel(
                workflows, [] { return ssa::domain::SsaPageRequest{}; });

            viewModel.exportFilteredList(QUrl::fromLocalFile("/tmp/ssa-export.csv"));

            QTRY_COMPARE_WITH_TIMEOUT(viewModel.running(), false, 1000);
            QCOMPARE(viewModel.lastSucceeded(), false);
            QCOMPARE(viewModel.lastStatus(), QString("failed"));
            QVERIFY(viewModel.lastMessage().contains("export failed"));
        }

        void export_view_model_passes_display_labels_explicitly() {
            auto exportPort = std::make_shared<CapturingExportPort>();
            auto workflows =
                std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
            ssa::presentation::ExportViewModel viewModel(workflows, [] {
                ssa::domain::SsaPageRequest request;
                request.visibleColumns = {"numero_ssa", "situacao"};
                return request;
            });

            viewModel.exportFilteredList(QUrl::fromLocalFile("/tmp/ssa-export-labels.csv"));

            QTRY_COMPARE_WITH_TIMEOUT(viewModel.running(), false, 1000);
            QCOMPARE(exportPort->request().query.visibleColumns,
                     std::vector<std::string>({"numero_ssa", "situacao"}));
            QCOMPARE(exportPort->request().headerLabels,
                     std::vector<std::string>({"No SSA", "Sit."}));
        }

        void advanced_text_filter_updates_only_the_changed_model_row() {
            ssa::presentation::filterpanel::FilterPanelAdvancedState state;
            ssa::presentation::AdvancedTextFilterViewModel textFilters(state);
            auto* model = qobject_cast<QAbstractItemModel*>(&textFilters);
            QVERIFY(model != nullptr);

            const auto roles = model->roleNames();
            const int keyRole = roles.key(QByteArrayLiteral("key"), -1);
            const int textFilterRole = roles.key(QByteArrayLiteral("textFilter"), -1);
            QVERIFY(keyRole >= Qt::UserRole);
            QVERIFY(textFilterRole >= Qt::UserRole);
            QCOMPARE(roles.size(), 6);
            for (const auto& roleName :
                 {QByteArrayLiteral("key"), QByteArrayLiteral("label"),
                  QByteArrayLiteral("labelShort"), QByteArrayLiteral("textFilter"),
                  QByteArrayLiteral("operatorIndex"), QByteArrayLiteral("operatorLabel")}) {
                QVERIFY(roles.values().contains(roleName));
            }

            int situationRow = -1;
            for (int row = 0; row < model->rowCount(); ++row) {
                if (model->data(model->index(row, 0), keyRole).toString() ==
                    QStringLiteral("situacao")) {
                    situationRow = row;
                    break;
                }
            }
            QVERIFY(situationRow >= 0);
            QSignalSpy changedSpy(model, &QAbstractItemModel::dataChanged);
            QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);

            textFilters.setTextFilter(QStringLiteral("situacao"), QStringLiteral("=APV"));

            QCOMPARE(changedSpy.count(), 1);
            QCOMPARE(resetSpy.count(), 0);
            const auto arguments = changedSpy.takeFirst();
            QCOMPARE(arguments.at(0).value<QModelIndex>().row(), situationRow);
            QCOMPARE(arguments.at(1).value<QModelIndex>().row(), situationRow);
            QVERIFY(arguments.at(2).value<QList<int>>().contains(textFilterRole));
            QCOMPARE(model->data(model->index(situationRow, 0), textFilterRole).toString(),
                     QString("=APV"));
        }

        void ssa_table_model_exposes_only_the_display_role() {
            ssa::presentation::SsaTableModel model("numero_ssa");
            populateTableModel(model);

            const auto roles = model.roleNames();
            QHash<int, QByteArray> expectedRoles;
            expectedRoles.insert(Qt::DisplayRole, QByteArrayLiteral("displayValue"));

            QCOMPARE(roles, expectedRoles);
        }

        void ssa_table_model_serializes_visible_row_values() {
            ssa::presentation::SsaTableModel model("numero_ssa");
            populateTableModel(model);

            QCOMPARE(model.rowText(0), QString("202500001"));
            QCOMPARE(model.rowText(-1), QString());
            QCOMPARE(model.rowText(1), QString());
        }

        void ssa_table_model_record_access_keeps_a_stable_reference() {
            ssa::presentation::SsaTableModel model("numero_ssa");
            populateTableModel(model);

            const auto first = model.recordAt(0);
            const auto second = model.recordAt(0);
            const auto* firstAddress = first ? std::addressof(*first) : nullptr;
            const auto* secondAddress = second ? std::addressof(*second) : nullptr;

            QVERIFY(firstAddress != nullptr);
            QVERIFY(secondAddress != nullptr);
            QCOMPARE(firstAddress, secondAddress);
        }

        void column_filter_apply_signal_reloads_table_with_responsible_execution_filter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* columns = qobject_cast<ssa::presentation::ColumnFilterViewModel*>(
                model.browse()->filters()->columns());
            QVERIFY(columns != nullptr);

            columns->applyFilterFor("responsavel_execucao", "Ana");

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));
            QCOMPARE(QString::fromStdString(request.columnFilters.at("responsavel_execucao")),
                     QString("Ana"));
        }

        void active_filter_removal_reloads_table_without_removed_column_filter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->filters()->setColumnFilters({{"responsavel_execucao", "Ana"}});
            QCOMPARE(model.browse()->filters()->removeActiveFilter(QVariantMap{
                         {QStringLiteral("kind"), QStringLiteral("column")},
                         {QStringLiteral("key"), QStringLiteral("responsavel_execucao")}}),
                     true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QVERIFY(!repository->requests().back().columnFilters.contains("responsavel_execucao"));
        }

        void manual_filter_state_summary_and_request_stay_in_sync() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->setQuickSector("MEG2");
            model.browse()->filters()->setColumnFilters({{"responsavel_execucao", "Ana"}});
            text->setTextFilter("situacao", "=APV");

            QCOMPARE(repository->requests().size(), std::size_t{0});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->filters()->activeFilterEntries().size(), 3,
                                      1000);
            QVERIFY(!activeFilterEntry(model.browse()->filters(), "advanced_text", "setor_executor")
                         .isEmpty());
            QVERIFY(!activeFilterEntry(model.browse()->filters(), "column", "responsavel_execucao")
                         .isEmpty());
            QVERIFY(!activeFilterEntry(model.browse()->filters(), "advanced_text", "situacao")
                         .isEmpty());
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("MEG2"));
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("Resp. Exec"));
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("Sit"));
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("APV"));

            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(QString::fromStdString(request.quickSector), QString(""));
            QVERIFY(request.advancedFilters.textFilters.contains("setor_executor"));
            QCOMPARE(
                QString::fromStdString(request.advancedFilters.textFilters.at("setor_executor")),
                QString("=MEG2"));
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));
            QCOMPARE(QString::fromStdString(request.columnFilters.at("responsavel_execucao")),
                     QString("Ana"));
            QVERIFY(request.advancedFilters.textFilters.contains("situacao"));
            QCOMPARE(QString::fromStdString(request.advancedFilters.textFilters.at("situacao")),
                     QString("=APV"));
        }

        void summary_removal_reloads_only_the_removed_filter_family() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->setQuickSector("MEG2");
            model.browse()->filters()->setColumnFilters({{"responsavel_execucao", "Ana"}});
            text->setTextFilter("situacao", "=APV");
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->filters()->activeFilterEntries().size(), 3,
                                      1000);

            const auto advancedEntry =
                activeFilterEntry(model.browse()->filters(), "advanced_text", "situacao");
            QVERIFY(!advancedEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(advancedEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            auto request = repository->requests().back();
            QVERIFY(!request.advancedFilters.textFilters.contains("situacao"));
            QCOMPARE(QString::fromStdString(request.quickSector), QString(""));
            QVERIFY(request.advancedFilters.textFilters.contains("setor_executor"));
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));

            const auto quickSectorEntry =
                activeFilterEntry(model.browse()->filters(), "advanced_text", "setor_executor");
            QVERIFY(!quickSectorEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(quickSectorEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            request = repository->requests().back();
            QCOMPARE(QString::fromStdString(request.quickSector), QString(""));
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));
            QVERIFY(!request.advancedFilters.textFilters.contains("setor_executor"));
            QVERIFY(!request.advancedFilters.textFilters.contains("situacao"));
        }

        void exclusion_filter_setter_reloads_table() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->filters()->setExcludeScaSesSte(true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(repository->requests().back().excludeScaSesSte, true);
        }

        void advanced_text_filter_selection_reloads_table() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            QCOMPARE(text->updateFilterWithSelectedValue("responsavel_execucao", "Ana"), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto textFilters = repository->requests().back().advancedFilters.textFilters;
            QVERIFY(textFilters.contains("responsavel_execucao"));
            QCOMPARE(QString::fromStdString(textFilters.at("responsavel_execucao")),
                     QString("=Ana"));
        }

        void advanced_text_filter_clear_reloads_table_without_filter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setTextFilter("responsavel_execucao", "=Ana");
            QCOMPARE(text->clearTextFilterAndApply("responsavel_execucao"), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QVERIFY(!repository->requests().back().advancedFilters.textFilters.contains(
                "responsavel_execucao"));
        }

        void column_filter_summary_uses_contains_marker() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("situacao");
            filters.setColumnValue("APV");
            filters.addColumnFilter();

            QCOMPARE(filters.activeFilters().contains("Sit: APV"), true);
            QCOMPARE(filters.activeFilterSummary().contains("Sit: APV"), true);
        }

        void advanced_filters_are_added_to_request_contract() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(text != nullptr);
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);

            week->setWeekColumnKey("semana_programada");
            week->setYearFilter("2025");
            week->setWeekFilter("2");
            text->setTextFilter("situacao", "=APV");
            text->setTextFilter("setor_executor", "=SMM");
            week->setIssueYearFilter("2025");
            week->setExecutionYearFilter("2026");
            derivation->setReprogrammingMode("gte");
            derivation->setReprogrammingValues({"1", "3", "5"});
            week->setIssueWeekStartFilter("202501");
            week->setIssueWeekEndFilter("202510");
            week->setExecutionWeekStartFilter("202601");
            week->setExecutionWeekEndFilter("202620");
            derivation->setDerivationMode("derived");
            derivation->setOnlyReprogrammed(true);
            QCOMPARE(repository->requests().size(), std::size_t{0});
            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(QString::fromStdString(request.advancedFilters.weekColumnKey),
                     QString("semana_programada"));
            QCOMPARE(request.advancedFilters.year.value_or(0), 2025);
            QCOMPARE(request.advancedFilters.week.value_or(0), 2);
            QVERIFY(request.advancedFilters.textFilters.at("situacao") == "=APV");
            QVERIFY(request.advancedFilters.textFilters.at("setor_executor") == "=SMM");
            QCOMPARE(request.advancedFilters.issueYear.value_or(0), 2025);
            QCOMPARE(request.advancedFilters.executionYear.value_or(0), 2026);
            QCOMPARE(request.advancedFilters.reprogrammingComparison,
                     ssa::domain::NumericComparisonMode::GreaterOrEqual);
            QVERIFY(request.advancedFilters.reprogrammingValues == (std::vector<int>{1, 3, 5}));
            QCOMPARE(request.advancedFilters.issueWeekStart.value_or(0), 202501);
            QCOMPARE(request.advancedFilters.issueWeekEnd.value_or(0), 202510);
            QCOMPARE(request.advancedFilters.executionWeekStart.value_or(0), 202601);
            QCOMPARE(request.advancedFilters.executionWeekEnd.value_or(0), 202620);
            QCOMPARE(request.advancedFilters.derivationMode,
                     ssa::domain::DerivationFilterMode::DerivedOnly);
            QCOMPARE(request.advancedFilters.onlyReprogrammed, false);
        }

        void advanced_text_filter_selection_builds_multi_value_terms() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            QCOMPARE(text->updateFilterWithSelectedValue("situacao", "APV"), true);
            text->setOperatorMode("situacao", "different");
            QCOMPARE(text->updateFilterWithSelectedValue("situacao", "ADM"), true);
            QCOMPARE(text->updateFilterWithSelectedValue("situacao", "APV"), false);

            QCOMPARE(text->textFilter("situacao"), QString("=APV,!=ADM"));
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));

            text->setOperatorMode("situacao", "different");
            QCOMPARE(text->operatorModeFor("situacao"), QString("different"));

            text->setOperatorMode("situacao", "mixed");
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));

            text->replaceWithOperatorValueLists("situacao", {}, {"APV", "ADM"});

            QCOMPARE(text->textFilter("situacao"), QString("!=APV,!=ADM"));

            text->replaceWithOperatorValueLists("situacao", {"SCA", "SES"}, {"STE"});

            QCOMPARE(text->textFilter("situacao"), QString("=SCA,=SES,!=STE"));
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));
        }

        void every_advanced_text_filter_syncs_column_summary_and_request() {
            auto keysRepository = std::make_shared<FakeRepository>();
            auto keysService = std::make_shared<ssa::query::SsaQueryService>(keysRepository);
            auto keysCommands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel keysModel(keysService, keysCommands);
            auto* keysAdvanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                keysModel.browse()->filters()->advanced());
            QVERIFY(keysAdvanced != nullptr);
            auto* keysText =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(keysAdvanced->text());
            QVERIFY(keysText != nullptr);

            const auto keys = ssa::tests::advancedTextFilterKeys(*keysText);
            QCOMPARE(keys.size(), 11);
            QVERIFY(!keys.contains(QStringLiteral("grau_prioridade_emissao")));
            QVERIFY(!keys.contains(QStringLiteral("grau_prioridade_planejamento")));

            for (const auto& key : keys) {
                auto repository = std::make_shared<FakeRepository>();
                auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
                auto commands = std::make_shared<FakeCommands>();
                ssa::presentation::MainViewModel model(service, commands);
                auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                    model.browse()->filters()->advanced());
                QVERIFY(advanced != nullptr);
                auto* text =
                    qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
                QVERIFY(text != nullptr);

                model.browse()->filters()->setColumnFilters({{key.toStdString(), "OLD"}});
                text->setTextFilter(key, "=IN,!OUT");

                QVERIFY2(!model.browse()->filters()->columnFilters().contains(key.toStdString()),
                         qPrintable(QString("column filter was not cleared for %1").arg(key)));
                QCOMPARE(text->textFilter(key), QString("=IN,!OUT"));
                QCOMPARE(ssa::tests::advancedTextFilterCardState(*text, key)
                             .value("textFilter")
                             .toString(),
                         QString("=IN,!OUT"));

                QTRY_VERIFY_WITH_TIMEOUT(
                    !activeFilterEntry(model.browse()->filters(), "advanced_text", key).isEmpty(),
                    1000);
                const auto entry =
                    activeFilterEntry(model.browse()->filters(), "advanced_text", key);
                QVERIFY2(!entry.isEmpty(),
                         qPrintable(QString("summary entry missing for %1").arg(key)));

                model.browse()->apply();
                QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
                auto request = repository->requests().back();
                QVERIFY2(!request.columnFilters.contains(key.toStdString()),
                         qPrintable(QString("request still has column filter for %1").arg(key)));
                QVERIFY2(request.advancedFilters.textFilters.contains(key.toStdString()),
                         qPrintable(QString("request missing advanced filter for %1").arg(key)));
                QCOMPARE(QString::fromStdString(
                             request.advancedFilters.textFilters.at(key.toStdString())),
                         QString("=IN,!OUT"));

                QCOMPARE(model.browse()->filters()->removeActiveFilter(entry), true);
                QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
                request = repository->requests().back();
                QVERIFY2(!request.advancedFilters.textFilters.contains(key.toStdString()),
                         qPrintable(QString("advanced filter was not removed for %1").arg(key)));
                QVERIFY2(!request.columnFilters.contains(key.toStdString()),
                         qPrintable(QString("column filter was restored for %1").arg(key)));
            }
        }

        void advanced_text_filter_clear_preserves_selected_operator() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setOperatorMode("situacao", "different");
            text->updateFilterWithSelectedValue("situacao", "ASE");
            text->setTextFilter("situacao", "");

            QCOMPARE(text->textFilter("situacao"), QString(""));
            QCOMPARE(text->operatorModeFor("situacao"), QString("different"));
        }

        void advanced_summary_removal_reloads_week_and_derivation_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);

            week->setIssueYearFilter("2026");
            derivation->setOnlyReprogrammed(true);
            QTRY_VERIFY_WITH_TIMEOUT(
                !activeFilterEntry(model.browse()->filters(), "advanced_issue_year").isEmpty(),
                1000);
            QTRY_VERIFY_WITH_TIMEOUT(
                !activeFilterEntry(model.browse()->filters(), "advanced_only_reprogrammed")
                     .isEmpty(),
                1000);

            const auto issueYearEntry =
                activeFilterEntry(model.browse()->filters(), "advanced_issue_year");
            QVERIFY(!issueYearEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(issueYearEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            auto request = repository->requests().back();
            QVERIFY(!request.advancedFilters.issueYear.has_value());
            QCOMPARE(request.advancedFilters.onlyReprogrammed, true);

            const auto reprogrammedEntry =
                activeFilterEntry(model.browse()->filters(), "advanced_only_reprogrammed");
            QVERIFY(!reprogrammedEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(reprogrammedEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            request = repository->requests().back();
            QVERIFY(!request.advancedFilters.issueYear.has_value());
            QCOMPARE(request.advancedFilters.onlyReprogrammed, false);
            QVERIFY(activeFilterEntry(model.browse()->filters(), "advanced_issue_year").isEmpty());
            QVERIFY(activeFilterEntry(model.browse()->filters(), "advanced_only_reprogrammed")
                        .isEmpty());
        }

        void advanced_submodels_update_shared_filter_state() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(text != nullptr);
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);

            text->updateFilterWithSelectedValue("situacao", "ASE");
            week->setIssueWeekStartFilter("202601");
            derivation->setDerivationMode("derived");

            QCOMPARE(text->textFilter("situacao"), QString("=ASE"));
            QCOMPARE(week->issueWeekStartFilter(), QString("202601"));
            QCOMPARE(derivation->derivationMode(), QString("derived"));
        }

        void clear_advanced_filters_keeps_column_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(text != nullptr);
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);
            auto* columns =
                qobject_cast<ssa::presentation::ColumnFilterViewModel*>(filters.columns());
            QVERIFY(columns != nullptr);

            columns->applyFilterFor("descricao_ssa", "bomba");
            filters.setQuickSector("MEG2");
            text->setTextFilter("situacao", "=APV");
            week->setIssueYearFilter("2025");
            derivation->setOnlyReprogrammed(true);
            advanced->clear();

            QVERIFY(filters.columnFilters().at("descricao_ssa") == "bomba");
            QCOMPARE(text->textFilter("situacao"), QString(""));
            QCOMPARE(week->issueYearFilter(), QString(""));
            QCOMPARE(derivation->onlyReprogrammed(), false);
            QCOMPARE(filters.quickSector(), QString(""));
            QCOMPARE(text->textFilter("setor_executor"), QString(""));
            QCOMPARE(qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector())
                         ->excludeScaSesSte(),
                     false);
        }

        void reset_filters_restores_default_sca_ses_ste_exclusion() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("setor_executor");
            filters.resetFilters();

            QCOMPARE(filters.columnKey(), QString("situacao"));
            QCOMPARE(qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector())
                         ->excludeScaSesSte(),
                     false);
            QCOMPARE(filters.activeFilters().contains("sem SCA/SES/STE"), false);
        }

        void column_filter_rows_publish_active_values() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* columns =
                qobject_cast<ssa::presentation::ColumnFilterViewModel*>(filters.columns());
            QVERIFY(columns != nullptr);

            columns->applyFilterFor("descricao_ssa", "bomba");

            QString publishedValue;
            for (const auto& row : columns->rows()) {
                const auto map = row.toMap();
                if (map.value("key").toString() == "descricao_ssa") {
                    publishedValue = map.value("value").toString();
                    break;
                }
            }

            QCOMPARE(publishedValue, QString("bomba"));
            filters.resetFilters();

            QString resetValue;
            for (const auto& row : columns->rows()) {
                const auto map = row.toMap();
                if (map.value("key").toString() == "descricao_ssa") {
                    resetValue = map.value("value").toString();
                    break;
                }
            }
            QCOMPARE(resetValue, QString(""));
        }

        void explicit_save_filters_button_persists_current_snapshot() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->filters()->setQuickSector("MEG2");
            QMetaObject::invokeMethod(model.preferenceFlow(), "savePreferences");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(QString::fromStdString(preferences->snapshot().filters.quickSector),
                     QString(""));
            QCOMPARE(QString::fromStdString(
                         preferences->snapshot().filters.advancedTextFilters.at("setor_executor")),
                     QString("=MEG2"));
        }

        void status_shortcuts_cycle_include_exclude_and_disabled() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->toggleStatusShortcut("APV");

            QCOMPARE(text->textFilter("situacao"), QString("=APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QCOMPARE(model.browse()->filters()->statusShortcutState("APV"), 1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(QString::fromStdString(
                         repository->requests().back().advancedFilters.textFilters.at("situacao")),
                     QString("=APV"));

            model.browse()->filters()->toggleStatusShortcut("STE");

            QCOMPARE(text->textFilter("situacao"), QString("=APV,=STE"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("STE"));
            QCOMPARE(model.browse()->filters()->statusShortcutState("APV"), 1);
            QCOMPARE(model.browse()->filters()->statusShortcutState("STE"), 1);

            model.browse()->filters()->toggleStatusShortcut("APV");

            QCOMPARE(text->textFilter("situacao"), QString("=STE,!=APV"));
            QVERIFY(!model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("STE"));
            QCOMPARE(model.browse()->filters()->statusShortcutState("APV"), 2);
            QCOMPARE(model.browse()->filters()->statusShortcutState("STE"), 1);
            // Verify the friendly "Exc:" label reaches activeFilterEntries (Python-style
            // formatting).
            {
                bool foundExcLabel = false;
                const auto entries = model.browse()->filters()->activeFilterEntries();
                for (const auto& entry : entries) {
                    const auto text = entry.toMap().value("text").toString();
                    if (text.contains("Exc:") && text.contains("APV")) {
                        foundExcLabel = true;
                        break;
                    }
                }
                QVERIFY2(foundExcLabel,
                         "activeFilterEntries should contain 'Exc: APV' after exclusion");
            }

            model.browse()->filters()->toggleStatusShortcut("APV");

            QCOMPARE(text->textFilter("situacao"), QString("=STE"));
            QVERIFY(!model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("STE"));
            QCOMPARE(model.browse()->filters()->statusShortcutState("APV"), 0);
            QCOMPARE(model.browse()->filters()->statusShortcutState("STE"), 1);
        }

        void status_shortcuts_do_not_mark_excluded_status_values() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setTextFilter("situacao", "!STE");

            QVERIFY(!model.browse()->filters()->statusShortcutSelected("STE"));

            text->setTextFilter("situacao", "=APV,!STE");

            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(!model.browse()->filters()->statusShortcutSelected("STE"));

            model.browse()->filters()->toggleStatusShortcut("STE");

            QCOMPARE(text->textFilter("situacao"), QString("=APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(!model.browse()->filters()->statusShortcutSelected("STE"));
        }

        void filter_history_restores_requested_levels_and_consumes_states() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);
            ssa::ports::UserPreferencesSnapshot baseline;
            baseline.filters.searchText = "A";
            baseline.filters.columnFilters = {{"situacao", "=APV"}};
            browse.applyPreferences(baseline);

            auto stateB = baseline;
            stateB.filters.searchText = "B";
            stateB.filters.quickSector.clear();
            stateB.filters.columnFilters = {{"situacao", "=STE"}};
            stateB.filters.advancedTextFilters = {{"descricao_ssa", "=Bomba"}};
            stateB.filters.advancedWeekColumnKey = "semana_executada";
            stateB.filters.advancedYear = "2025";
            stateB.filters.advancedWeek = "3";
            stateB.filters.issueYear = "2026";
            stateB.filters.executionYear = "2027";
            stateB.filters.reprogrammingMode = "lte";
            stateB.filters.reprogrammingValues = "1,3,5";
            stateB.filters.issueWeekStart = "202601";
            stateB.filters.issueWeekEnd = "202620";
            stateB.filters.executionWeekStart = "202701";
            stateB.filters.executionWeekEnd = "202720";
            stateB.filters.derivationMode = "derived";
            stateB.filters.excludeScaSesSte = false;
            stateB.filters.onlyReprogrammed = false;
            browse.search()->setText("B");
            browse.filters()->applyPreferences(stateB);
            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);

            auto stateC = stateB;
            stateC.filters.searchText = "C";
            stateC.filters.columnFilters = {{"situacao", "=SCA"}};
            stateC.filters.advancedTextFilters = {{"descricao_ssa", "=Motor"}};
            stateC.filters.advancedWeek = "4";
            stateC.filters.reprogrammingValues = "2,4";
            stateC.filters.derivationMode = "root";
            browse.search()->setText("C");
            browse.filters()->applyPreferences(stateC);
            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QCOMPARE(browse.filterUndoDepth(), 2);

            QSignalSpy preferencesSaveSpy(
                &browse, &ssa::presentation::BrowseViewModel::preferencesSaveRequested);
            browse.undoFilterLevels(1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{3}, 1000);
            QCOMPARE(browse.search()->text(), QString("B"));
            QCOMPARE(browse.filters()->columnFilters().at("situacao"), std::string("=STE"));
            ssa::ports::UserPreferencesSnapshot restored;
            browse.writePreferences(restored);
            QVERIFY(restored.filters == stateB.filters);
            QCOMPARE(browse.filterUndoDepth(), 1);
            QCOMPARE(preferencesSaveSpy.count(), 1);

            browse.undoFilters();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{4}, 1000);
            QCOMPARE(browse.search()->text(), QString("A"));
            QCOMPARE(browse.filterUndoDepth(), 0);
            QCOMPARE(preferencesSaveSpy.count(), 2);

            QTRY_VERIFY_WITH_TIMEOUT(!browse.backgroundWorkRunning(), 1000);
            const auto startedRequestCountBeforeInvalidUndo = repository->startedRequests().size();
            browse.undoFilterLevels(1);
            QVERIFY(!browse.backgroundWorkRunning());
            QCOMPARE(repository->startedRequests().size(), startedRequestCountBeforeInvalidUndo);
            QCOMPARE(repository->requests().size(), std::size_t{4});
            QCOMPARE(preferencesSaveSpy.count(), 2);
        }

        void filter_history_can_restore_two_levels_in_one_query() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);
            ssa::ports::UserPreferencesSnapshot state;
            state.filters.searchText = "A";
            browse.applyPreferences(state);

            std::size_t expectedRequests = 0;
            for (const auto* search : {"B", "C"}) {
                browse.search()->setText(search);
                browse.apply();
                ++expectedRequests;
                QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), expectedRequests, 1000);
            }

            browse.undoFilterLevels(2);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{3}, 1000);
            QCOMPARE(browse.search()->text(), QString("A"));
            QCOMPARE(browse.filterUndoDepth(), 0);
        }

        void filter_history_is_deduplicated_bounded_and_reset_by_preferences() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);
            ssa::ports::UserPreferencesSnapshot state;
            state.filters.searchText = "0";
            browse.applyPreferences(state);

            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(browse.filterUndoDepth(), 0);

            for (int index = 1; index <= 11; ++index) {
                browse.search()->setText(QString::number(index));
                browse.apply();
                QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(),
                                          static_cast<std::size_t>(index + 1), 1000);
                QTRY_VERIFY_WITH_TIMEOUT(!browse.status()->loading(), 1000);
            }
            QCOMPARE(browse.filterUndoDepth(), 10);

            QTRY_VERIFY_WITH_TIMEOUT(!browse.backgroundWorkRunning(), 1000);
            const auto requestCountBeforeRepeatedApply = repository->requests().size();
            const auto startedRequestCountBeforeRepeatedApply =
                repository->startedRequests().size();
            browse.apply();
            QVERIFY(!browse.status()->loading());
            QTRY_VERIFY_WITH_TIMEOUT(!browse.backgroundWorkRunning(), 1000);
            QCOMPARE(repository->requests().size(), requestCountBeforeRepeatedApply);
            QCOMPARE(repository->startedRequests().size(), startedRequestCountBeforeRepeatedApply);
            QCOMPARE(browse.filterUndoDepth(), 10);

            const auto requestCountBeforeUndo = repository->requests().size();
            const auto startedRequestCountBeforeUndo = repository->startedRequests().size();
            browse.undoFilterLevels(10);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), requestCountBeforeUndo + 1,
                                      1000);
            QTRY_VERIFY_WITH_TIMEOUT(!browse.backgroundWorkRunning(), 1000);
            QCOMPARE(repository->requests().size(), requestCountBeforeUndo + 1);
            QCOMPARE(repository->startedRequests().size(), startedRequestCountBeforeUndo + 1);
            QCOMPARE(browse.search()->text(), QString("1"));
            QCOMPARE(browse.filterUndoDepth(), 0);

            state.filters.searchText = "baseline";
            browse.applyPreferences(state);
            QCOMPARE(browse.filterUndoDepth(), 0);
            QCOMPARE(browse.canUndoFilters(), false);
        }

        void filter_history_text_contains_only_filter_conditions() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);
            ssa::ports::UserPreferencesSnapshot state;
            state.filters.searchText = "A";
            state.filters.quickSector = "MEG2";
            state.filters.columnFilters = {{"situacao", "=APV"}};
            browse.applyPreferences(state);

            browse.search()->setText("B");
            browse.apply();

            const auto history = browse.filterHistoryText();
            QVERIFY(history.contains("search=A"));
            QVERIFY(history.contains("quickSector="));
            QVERIFY(history.contains("situacao:=APV"));
            QVERIFY(!history.contains("rows"));
            QVERIFY(!history.contains("results"));
        }

        void failed_filter_keeps_conditions_and_previous_table_for_retry_and_undo() {
            auto repository = std::make_shared<FailSecondPageRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);

            browse.search()->setText("A");
            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(browse.tableModel()->recordAt(0) != nullptr, 1000);
            QCOMPARE(std::string(browse.tableModel()->recordAt(0)->valueOf("descricao_ssa")),
                     std::string{"A"});

            browse.search()->setText("B");
            QTest::ignoreMessage(QtWarningMsg, "Page query failed: filter B failed rc=11");
            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(browse.status()->error(),
                                      QString("Falha ao acessar o banco de dados"), 1000);
            QVERIFY(!browse.status()->error().contains("rc="));

            QCOMPARE(browse.search()->text(), QString("B"));
            QCOMPARE(browse.currentRequest().searchText, std::string{"B"});
            QCOMPARE(std::string(browse.tableModel()->recordAt(0)->valueOf("descricao_ssa")),
                     std::string{"A"});
            QCOMPARE(browse.status()->message(),
                     QString("Falha ao consultar dados; tabela ainda mostra o resultado anterior"));
            QVERIFY(browse.canUndoFilters());

            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{3}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                std::string(browse.tableModel()->recordAt(0)->valueOf("descricao_ssa")),
                std::string{"B"}, 1000);

            browse.undoFilters();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{4}, 1000);
            QCOMPARE(browse.search()->text(), QString("A"));
            QTRY_COMPARE_WITH_TIMEOUT(
                std::string(browse.tableModel()->recordAt(0)->valueOf("descricao_ssa")),
                std::string{"A"}, 1000);
        }

        void generic_page_failure_logs_diagnostic_and_redacts_public_error() {
            auto repository = std::make_shared<FailSecondPageRepository>(true);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);

            browse.search()->setText("A");
            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(browse.tableModel()->recordAt(0) != nullptr, 1000);

            browse.search()->setText("B");
            QTest::ignoreMessage(QtWarningMsg,
                                 "Page query failed: filter B path=/tmp/private.sqlite rc=11");
            browse.apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(browse.status()->error(), QString("Falha ao consultar dados"),
                                      1000);
            QVERIFY(!browse.status()->error().contains("private.sqlite"));
            QCOMPARE(browse.search()->text(), QString("B"));
            QCOMPARE(std::string(browse.tableModel()->recordAt(0)->valueOf("descricao_ssa")),
                     std::string{"A"});
        }

        void clear_search_applies_once_and_is_undoable() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);
            ssa::ports::UserPreferencesSnapshot baseline;
            baseline.filters.searchText = "active";
            browse.applyPreferences(baseline);

            browse.search()->clear();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(repository->requests().back().searchText, std::string());
            QCOMPARE(browse.property("canUndoFilters").toBool(), true);
        }

        void clear_search_slot_clears_and_applies_once() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);
            ssa::ports::UserPreferencesSnapshot baseline;
            baseline.filters.searchText = "active";
            browse.applyPreferences(baseline);

            browse.clearSearchAndResetPage();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(browse.search()->text(), QString());
            QCOMPARE(repository->requests().back().searchText, std::string());
        }

        void applying_saved_filter_is_undoable() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->search()->setText("saved");
            QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                      Q_ARG(QString, QString("saved-filter")));

            model.browse()->search()->setText("previous");
            model.browse()->apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);

            QMetaObject::invokeMethod(model.preferenceFlow(), "applySavedFilter",
                                      Q_ARG(QString, QString("saved-filter")));
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QCOMPARE(model.browse()->search()->text(), QString("saved"));
            QCOMPARE(model.browse()->canUndoFilters(), true);

            model.browse()->undoFilters();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{3}, 1000);
            QCOMPARE(model.browse()->search()->text(), QString("previous"));
        }

        void filter_undo_starts_immediately_and_only_latest_request_publishes() {
            auto repository = std::make_shared<FakeRepository>(
                ssa::tests::presentation_smoke::FakeRepositoryConfig{
                    .delay = std::chrono::milliseconds{300}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::BrowseViewModel browse(service);
            ssa::ports::UserPreferencesSnapshot baseline;
            baseline.filters.searchText = "A";
            browse.applyPreferences(baseline);
            QSignalSpy pageChangedSpy(&browse, &ssa::presentation::BrowseViewModel::pageChanged);

            browse.search()->setText("B");
            browse.apply();
            QTRY_COMPARE_WITH_TIMEOUT(repository->startedRequests().size(), std::size_t{1}, 1000);

            browse.undoFilters();

            QTRY_COMPARE_WITH_TIMEOUT(repository->startedRequests().size(), std::size_t{2}, 150);
            QCOMPARE(repository->requests().size(), std::size_t{0});
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(pageChangedSpy.count(), 1, 1000);
            const auto* published = browse.tableModel()->recordAt(0);
            QVERIFY(published != nullptr);
            QCOMPARE(std::string(published->valueOf("descricao_ssa")), std::string("A"));
            QCOMPARE(repository->startedRequests().at(0).searchText, std::string("B"));
            QCOMPARE(repository->startedRequests().at(1).searchText, std::string("A"));
            QCOMPARE(browse.search()->text(), QString("A"));
        }

        void named_saved_filter_persists_applies_and_removes_current_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->search()->setText("bomba");
            model.browse()->filters()->setQuickSector("MEG2");
            QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                      Q_ARG(QString, QString("braba")));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(preferences->snapshot().savedFilters.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(preferences->snapshot().savedFilters.front().name),
                     QString("braba"));
            QCOMPARE(QString::fromStdString(
                         preferences->snapshot().savedFilters.front().filters.searchText),
                     QString("bomba"));

            model.browse()->search()->setText("outra");
            model.browse()->filters()->setQuickSector("MMU3");
            QMetaObject::invokeMethod(model.preferenceFlow(), "applySavedFilter",
                                      Q_ARG(QString, QString("braba")));

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(model.browse()->search()->text(), QString("bomba"));
            QCOMPARE(model.browse()->filters()->quickSector(), QString(""));
            QVERIFY(repository->requests().back().advancedFilters.textFilters.contains(
                "setor_executor"));
            QCOMPARE(
                QString::fromStdString(
                    repository->requests().back().advancedFilters.textFilters.at("setor_executor")),
                QString("=MEG2"));

            QMetaObject::invokeMethod(model.preferenceFlow(), "removeSavedFilter",
                                      Q_ARG(QString, QString("braba")));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().savedFilters.size(), std::size_t{0},
                                      1000);
        }

        void named_saved_filter_rejects_name_longer_than_128_characters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);
            model.browse()->search()->setText("active");

            QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                      Q_ARG(QString, QString(129, 'n')));

            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 0);
            QCOMPARE(preferences->saveCount(), 0);
            QVERIFY(model.browse()->status()->error().contains("128"));
        }

        void named_saved_filter_rejects_more_than_200_filters() {
            ssa::ports::UserPreferencesSnapshot initial;
            for (int index = 0; index < 200; ++index) {
                initial.savedFilters.push_back({"filter-" + std::to_string(index), {}});
            }
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);
            model.browse()->search()->setText("active");

            QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                      Q_ARG(QString, QString("filter-201")));

            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 200);
            QCOMPARE(preferences->saveCount(), 0);
            QVERIFY(model.browse()->status()->error().contains("200"));
        }

        void named_saved_filter_normalizes_overlapping_filter_families() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->setQuickSector("IEE3");
            model.browse()->filters()->setColumnFilters({{"situacao", "=APV"}});
            text->setTextFilter("setor_executor", "=IEE1");
            text->setTextFilter("situacao", "=SCA");
            model.browse()->filters()->setExcludeScaSesSte(true);
            QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                      Q_ARG(QString, QString("normalizado")));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(preferences->snapshot().savedFilters.size(), std::size_t{1});
            const auto saved = preferences->snapshot().savedFilters.front().filters;
            QVERIFY(saved.quickSector.empty());
            QVERIFY(saved.columnFilters.empty());
            QVERIFY(!saved.excludeScaSesSte);
            QCOMPARE(QString::fromStdString(saved.advancedTextFilters.at("setor_executor")),
                     QString("=IEE1"));
            QCOMPARE(QString::fromStdString(saved.advancedTextFilters.at("situacao")),
                     QString("=SCA"));
        }

        void filter_preferences_normalizer_canonicalizes_snapshot_without_viewmodel() {
            ssa::ports::FilterPreferencesSnapshot filters;
            filters.quickSector = "IEE3";
            filters.columnFilters = {{"setor_executor", "=IEE1"}, {"situacao", "=APV"}};
            filters.advancedTextFilters = {{"situacao", "=SCA"}};
            filters.excludeScaSesSte = true;

            ssa::presentation::normalizeFilterPreferences(filters);

            QVERIFY(filters.quickSector.empty());
            QVERIFY(filters.columnFilters.empty());
            QVERIFY(!filters.columnFilters.contains("situacao"));
            QVERIFY(!filters.excludeScaSesSte);
            QCOMPARE(QString::fromStdString(filters.advancedTextFilters.at("setor_executor")),
                     QString("=IEE3"));
            QCOMPARE(QString::fromStdString(filters.advancedTextFilters.at("situacao")),
                     QString("=SCA"));
        }

        void filter_preset_export_uses_current_filters_without_search_text() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            auto presets = std::make_shared<FakeFilterPresetStore>();
            ssa::presentation::MainViewModel model(service, commands, preferences, presets);

            model.browse()->search()->setText("nao exportar");
            model.browse()->filters()->setQuickSector("MEG2");
            QMetaObject::invokeMethod(
                model.preferenceFlow(), "exportFilterPreset",
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json")));

            QTRY_COMPARE_WITH_TIMEOUT(presets->saveCount(), 1, 1000);
            QCOMPARE(QString::fromStdString(presets->saved().filters.quickSector), QString(""));
            QCOMPARE(QString::fromStdString(
                         presets->saved().filters.advancedTextFilters.at("setor_executor")),
                     QString("=MEG2"));
            QCOMPARE(QString::fromStdString(presets->saved().filters.searchText), QString(""));
        }

        void filter_preset_import_preserves_search_text_and_applies_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            auto presets = std::make_shared<FakeFilterPresetStore>();
            ssa::ports::FilterPresetSnapshot preset;
            preset.filters.quickSector = "MMU3";
            preset.filters.columnFilters = {{"situacao", "=APV"}};
            presets->setNextLoad(preset);
            ssa::presentation::MainViewModel model(service, commands, preferences, presets);

            model.browse()->search()->setText("manter busca");
            QMetaObject::invokeMethod(
                model.preferenceFlow(), "importFilterPreset",
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json")));

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(model.browse()->search()->text(), QString("manter busca"));
            QCOMPARE(model.browse()->filters()->quickSector(), QString(""));
            QCOMPARE(model.browse()->filters()->columnFilters().at("situacao"),
                     std::string("=APV"));
            QCOMPARE(
                QString::fromStdString(
                    repository->requests().back().advancedFilters.textFilters.at("setor_executor")),
                QString("=MMU3"));
        }

        void filter_summary_removal_preserves_sort_and_visible_columns() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 160}, {"situacao", 140}};
            initial.sortColumnKey = "situacao";
            initial.sortAscending = true;
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->filters()->setColumnFilters({{"situacao", "=APV"}});

            QCOMPARE(model.browse()->filters()->removeActiveFilter(
                         QVariantMap{{QStringLiteral("kind"), QStringLiteral("column")},
                                     {QStringLiteral("key"), QStringLiteral("situacao")}}),
                     true);

            QVERIFY(model.browse()->filters()->columnFilters().empty());
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);
            const auto visibleColumns = model.browse()->visibleColumns();
            QCOMPARE(visibleColumns.size(), std::size_t{2});
            QCOMPARE(QString::fromStdString(visibleColumns.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(visibleColumns.at(1)), QString("situacao"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationFilterSmokeTest)

#include "PresentationFilterSmokeTest.moc"
