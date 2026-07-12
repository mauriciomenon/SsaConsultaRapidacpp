#include "PresentationSmokeFakes.h"

#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/ExportViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelDistinctValueFetcher.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/FilterPreferencesNormalizer.h"
#include "presentation/MainViewModel.h"
#include "presentation/PageQueryCoordinator.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableModel.h"
#include "presentation/WorkflowCommandRunner.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QVariantMap>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>

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
                                        const bool failDistinct = false)
            : blockDistinct_(blockDistinct), failDistinct_(failDistinct) {}

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
            {
                const std::scoped_lock lock(metricsMutex_);
                distinctThread_ = std::this_thread::get_id();
            }
            metricsStarted_.store(true, std::memory_order_release);
            if (blockDistinct_) {
                while (!stopToken.stop_requested()) {
                    std::this_thread::yield();
                }
                metricsFinished_.store(true, std::memory_order_release);
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

        [[nodiscard]] bool metricsShareWorkerThread() const {
            const std::scoped_lock lock(metricsMutex_);
            return distinctThread_ != std::thread::id{} && distinctThread_ == maxLengthThread_;
        }

      private:
        bool blockDistinct_{false};
        bool failDistinct_{false};
        mutable std::atomic_bool started_{false};
        mutable std::atomic_bool finished_{false};
        mutable std::atomic_bool metricsStarted_{false};
        mutable std::atomic_bool metricsFinished_{false};
        mutable std::mutex metricsMutex_;
        mutable std::thread::id distinctThread_;
        mutable std::thread::id maxLengthThread_;
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
            if (request.limit != ssa::domain::kAdvancedDistinctValuesLimit ||
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

        void setValues(std::vector<std::string> values) {
            const std::scoped_lock lock(mutex_);
            values_ = std::move(values);
        }

        [[nodiscard]] int advancedRequests() const {
            const std::scoped_lock lock(mutex_);
            return advancedRequests_;
        }

      private:
        mutable std::mutex mutex_;
        mutable std::vector<std::string> values_{"A", "Longest"};
        mutable int failedRequestsRemaining_{0};
        mutable int advancedRequests_{0};
    };

    class BlockingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest&) override {
            return run();
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest&) override {
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

    [[nodiscard]] QVariantMap
    cardStateFor(const ssa::presentation::AdvancedTextFilterViewModel& textFilters,
                 const QString& key) {
        for (const auto& state : textFilters.cardStates()) {
            const auto map = state.toMap();
            if (map.value(QStringLiteral("key")).toString() == key) {
                return map;
            }
        }
        return {};
    }

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

            filterPanel.refreshColumnValueOptionsFor(QStringLiteral("situacao"));

            QTRY_COMPARE_WITH_TIMEOUT(repository->advancedRequests(), 2, 1000);
            const QStringList expectedValues{QStringLiteral("A"), QStringLiteral("Longest")};
            QTRY_COMPARE_WITH_TIMEOUT(filterPanel.columnValueOptionsFor(QStringLiteral("situacao")),
                                      expectedValues, 1000);
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
            connect(
                &fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesReady, this,
                [&](std::uint64_t, const std::vector<std::string>&, std::size_t) { ++readyCount; });
            connect(&fetcher, &ssa::presentation::FilterPanelDistinctValueFetcher::valuesFailed,
                    this, [&](const std::uint64_t requestToken) {
                        QCOMPARE(requestToken, 1);
                        failed = true;
                    });
            ssa::domain::DistinctValuesRequest request;
            request.columnKey = "situacao";
            QTest::ignoreMessage(QtWarningMsg, "Column value query failed: distinct failed");

            fetcher.requestValues(request, 1, true);

            QTRY_VERIFY_WITH_TIMEOUT(failed, 1000);
            QCOMPARE(readyCount, 0);
        }

        void distinct_value_fetcher_destructor_drains_worker_without_callback() {
            auto repository = std::make_shared<BlockingPageRepository>(true, false);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            int callbackCount = 0;
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
            }

            QVERIFY(repository->metricsFinished());
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
        }

        void page_query_destructor_drains_the_canceled_worker() {
            auto repository = std::make_shared<BlockingPageRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            int callbackCount = 0;

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
            }

            QVERIFY(repository->finished());
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
        }

        void workflow_runner_destructor_drains_the_worker() {
            auto importPort = std::make_shared<BlockingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            int callbackCount = 0;

            {
                ssa::presentation::WorkflowCommandRunner runner(workflows);
                connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                        [&callbackCount] { ++callbackCount; });
                runner.rescan(ssa::ports::RescanMode::Incremental);
                QTRY_VERIFY_WITH_TIMEOUT(importPort->started(), 1000);
            }

            QVERIFY(importPort->finished());
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
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

        void ssa_table_owns_one_context_menu_outside_the_cell_delegate() {
            QDir repositoryRoot = QFileInfo(QString::fromUtf8(__FILE__)).dir();
            QVERIFY(repositoryRoot.cdUp());
            QVERIFY(repositoryRoot.cdUp());
            QFile qmlFile(repositoryRoot.filePath("app/desktop/qml/components/SsaTable.qml"));
            QVERIFY2(qmlFile.open(QIODevice::ReadOnly), qPrintable(qmlFile.errorString()));
            const QByteArray source = qmlFile.readAll();
            const auto delegatePosition = source.indexOf("delegate: Rectangle");
            const auto menuPosition = source.indexOf("id: cellContextMenu");

            QVERIFY(delegatePosition >= 0);
            QVERIFY(menuPosition >= 0);
            QCOMPARE(source.count("id: cellContextMenu"), 1);
            QVERIFY(menuPosition < delegatePosition);
            QVERIFY(!source.contains("rowSsaNumber"));
            QVERIFY(source.contains(
                "const menuParent = Overlay.overlay !== null ? Overlay.overlay : root;"));
            QVERIFY(source.contains("cellContextMenu.parent = menuParent;"));
        }

        void advanced_week_cards_reject_invalid_visible_values_before_apply() {
            QDir repositoryRoot = QFileInfo(QString::fromUtf8(__FILE__)).dir();
            QVERIFY(repositoryRoot.cdUp());
            QVERIFY(repositoryRoot.cdUp());
            const QStringList cardFiles{
                "app/desktop/qml/components/AdvancedWeekEmissionCard.qml",
                "app/desktop/qml/components/AdvancedWeekExecutionCard.qml",
            };

            for (const auto& cardFile : cardFiles) {
                QFile qmlFile(repositoryRoot.filePath(cardFile));
                QVERIFY2(qmlFile.open(QIODevice::ReadOnly), qPrintable(qmlFile.errorString()));
                const QByteArray source = qmlFile.readAll();
                const auto guardPosition =
                    source.indexOf("if (!root.week.isYearWeekValid(root.rangeStartDraft)");
                const auto applyPosition = source.indexOf("root.applyRequested();");

                QVERIFY2(guardPosition >= 0, qPrintable(cardFile));
                QVERIFY2(applyPosition > guardPosition, qPrintable(cardFile));
            }
        }

        void advanced_week_cards_keep_intermediate_input_outside_the_view_model() {
            QDir repositoryRoot = QFileInfo(QString::fromUtf8(__FILE__)).dir();
            QVERIFY(repositoryRoot.cdUp());
            QVERIFY(repositoryRoot.cdUp());
            const QStringList cardFiles{
                "app/desktop/qml/components/AdvancedWeekEmissionCard.qml",
                "app/desktop/qml/components/AdvancedWeekExecutionCard.qml",
            };

            for (const auto& cardFile : cardFiles) {
                QFile qmlFile(repositoryRoot.filePath(cardFile));
                QVERIFY2(qmlFile.open(QIODevice::ReadOnly), qPrintable(qmlFile.errorString()));
                const QByteArray source = qmlFile.readAll();

                QVERIFY2(source.contains("property string rangeStartDraft"), qPrintable(cardFile));
                QVERIFY2(source.contains("property string rangeEndDraft"), qPrintable(cardFile));
                const auto startDraftPosition =
                    source.indexOf("onTextEdited: root.rangeStartDraft = text");
                const auto endDraftPosition =
                    source.indexOf("onTextEdited: root.rangeEndDraft = text");
                QVERIFY2(startDraftPosition >= 0, qPrintable(cardFile));
                QVERIFY2(endDraftPosition >= 0, qPrintable(cardFile));
            }
        }

        void macro_report_database_values_use_plain_text() {
            QDir repositoryRoot = QFileInfo(QString::fromUtf8(__FILE__)).dir();
            QVERIFY(repositoryRoot.cdUp());
            QVERIFY(repositoryRoot.cdUp());
            QFile qmlFile(
                repositoryRoot.filePath("app/desktop/qml/components/AdvancedMacroFilterCard.qml"));
            QVERIFY2(qmlFile.open(QIODevice::ReadOnly), qPrintable(qmlFile.errorString()));
            const QByteArray source = qmlFile.readAll();

            QCOMPARE(source.count("textFormat: Text.PlainText"), 4);
        }

        void smoke_details_timeout_reports_capture_failure() {
            QDir repositoryRoot = QFileInfo(QString::fromUtf8(__FILE__)).dir();
            QVERIFY(repositoryRoot.cdUp());
            QVERIFY(repositoryRoot.cdUp());
            QFile qmlFile(
                repositoryRoot.filePath("app/desktop/qml/components/SmokeCaptureBridge.qml"));
            QVERIFY2(qmlFile.open(QIODevice::ReadOnly), qPrintable(qmlFile.errorString()));
            const QByteArray source = qmlFile.readAll();

            QVERIFY(source.contains("root.smokeController.reportCaptureFailure();"));
            QVERIFY(source.contains("root.smokeController.reportDetailsReady();"));
            QVERIFY(!source.contains("console.warn(\"Smoke capture: details window was not ready"));
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
            derivation->setReprogrammingEqualsFilter("1");
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
            QCOMPARE(request.advancedFilters.reprogrammingEquals.value_or(0), 1);
            QCOMPARE(request.advancedFilters.reprogrammingComparison,
                     ssa::domain::NumericComparisonMode::GreaterOrEqual);
            QVERIFY(request.advancedFilters.reprogrammingValues == (std::vector<int>{1, 3, 5}));
            QCOMPARE(request.advancedFilters.issueWeekStart.value_or(0), 202501);
            QCOMPARE(request.advancedFilters.issueWeekEnd.value_or(0), 202510);
            QCOMPARE(request.advancedFilters.executionWeekStart.value_or(0), 202601);
            QCOMPARE(request.advancedFilters.executionWeekEnd.value_or(0), 202620);
            QCOMPARE(request.advancedFilters.derivationMode,
                     ssa::domain::DerivationFilterMode::DerivedOnly);
            QCOMPARE(request.advancedFilters.onlyReprogrammed, true);
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

            QCOMPARE(text->textFilter("situacao"), QString("=APV,!ADM"));
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));

            text->setOperatorMode("situacao", "different");
            QCOMPARE(text->operatorModeFor("situacao"), QString("different"));

            text->setOperatorMode("situacao", "mixed");
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));

            text->replaceWithOperatorValueList("situacao", {"APV", "ADM"}, "different");

            QCOMPARE(text->textFilter("situacao"), QString("!APV,!ADM"));

            text->replaceWithOperatorValueLists("situacao", {"SCA", "SES"}, {"STE"});

            QCOMPARE(text->textFilter("situacao"), QString("=SCA,=SES,!STE"));
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

            QStringList keys;
            const auto rows = keysText->rows();
            std::ranges::transform(rows, std::back_inserter(keys), [](const auto& row) {
                return row.toMap().value(QStringLiteral("key")).toString();
            });
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
                QCOMPARE(cardStateFor(*text, key).value("textFilter").toString(),
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

            QCOMPARE(text->textFilter("situacao"), QString("=STE,!APV"));
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
