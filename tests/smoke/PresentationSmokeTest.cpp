#include "PresentationSmokeFakes.h"
#include "qt/FilesystemPath.h"

#include "application/ActivityAnalyticsService.h"
#include "application/SsaWorkflowService.h"
#include "domain/SsaTypes.h"
#include "ports/IActivityAnalyticsPort.h"
#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/DerivadasGraphModel.h"
#include "presentation/DetailsViewModel.h"
#include "presentation/ExportViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/MainPreferenceFlowCoordinator.h"
#include "presentation/MainViewModel.h"
#include "presentation/PageQueryCoordinator.h"
#include "presentation/RecentLogModel.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaRecordValueFormatter.h"
#include "presentation/StatusViewModel.h"
#include "query/SsaQueryService.h"

#include <QChar>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QRectF>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSizeF>
#include <QString>
#include <QTest>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

    using ssa::tests::presentation_smoke::CapturingDerivadasPort;
    using ssa::tests::presentation_smoke::CapturingImportPort;
    using ssa::tests::presentation_smoke::CapturingMaintenancePort;
    using ssa::tests::presentation_smoke::FakeCommands;
    using ssa::tests::presentation_smoke::FakePreferences;
    using ssa::tests::presentation_smoke::FakeRepository;
    using ssa::tests::presentation_smoke::FakeRepositoryConfig;

    class ThreadCapturingCommands final : public ssa::ports::IExternalCommandPort {
      public:
        ssa::ports::ExternalCommandResult execute(const ssa::ports::ExternalCommand&) override {
            const std::scoped_lock lock(mutex_);
            executedThread_ = QThread::currentThread();
            return {ssa::ports::ExternalCommandStatus::Succeeded, "ok"};
        }

        [[nodiscard]] QThread* executedThread() const {
            const std::scoped_lock lock(mutex_);
            return executedThread_;
        }

      private:
        mutable std::mutex mutex_;
        QThread* executedThread_{nullptr};
    };

    class BlockingActivityAnalyticsPort final : public ssa::ports::IActivityAnalyticsPort {
      public:
        explicit BlockingActivityAnalyticsPort(const bool blockSeries = false)
            : blockSeries_(blockSeries) {}

        ssa::domain::AnalyticsSeriesResult series(const ssa::domain::AnalyticsRequest&,
                                                  std::stop_token stopToken = {}) const override {
            started_.store(true, std::memory_order_release);
            while (blockSeries_ && !stopToken.stop_requested()) {
                std::this_thread::yield();
            }
            stopObserved_.store(stopToken.stop_requested(), std::memory_order_release);
            return {};
        }

        ssa::domain::AnalyticsDimensionValues dimensionValues(const ssa::domain::AnalyticsRequest&,
                                                              std::stop_token = {}) const override {
            return {};
        }

        std::vector<ssa::domain::AnalyticsMetricAvailability>
        availability(std::stop_token = {}) const override {
            return {};
        }

        [[nodiscard]] bool started() const noexcept {
            return started_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool stopObserved() const noexcept {
            return stopObserved_.load(std::memory_order_acquire);
        }

      private:
        bool blockSeries_{false};
        mutable std::atomic_bool started_{false};
        mutable std::atomic_bool stopObserved_{false};
    };

    class BlockingCancelableExportPort final : public ssa::ports::IExportPort {
      public:
        ssa::ports::WorkflowResult
        exportFilteredList(const ssa::ports::ExportFilteredListRequest&,
                           const std::stop_token stopToken = {}) override {
            started_.store(true, std::memory_order_release);
            std::unique_lock lock(mutex_);
            condition_.wait_for(lock, stopToken, std::chrono::seconds{2}, [] { return false; });
            const bool stopObserved = stopToken.stop_requested();
            stopObserved_.store(stopObserved, std::memory_order_release);
            stopWaitTimedOut_.store(!stopObserved, std::memory_order_release);
            const bool released =
                condition_.wait_for(lock, std::chrono::seconds{2}, [this] { return release_; });
            terminalTimedOut_.store(!released, std::memory_order_release);
            lock.unlock();
            if (!stopObserved) {
                return {ssa::ports::WorkflowStatus::Failed, "export stop wait timed out"};
            }
            finished_.store(true, std::memory_order_release);
            return {ssa::ports::WorkflowStatus::Failed, "export canceled"};
        }

        void releaseTerminal() {
            {
                const std::scoped_lock lock(mutex_);
                release_ = true;
            }
            condition_.notify_all();
        }

        [[nodiscard]] bool started() const {
            return started_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool finished() const {
            return finished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool stopObserved() const {
            return stopObserved_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool stopWaitTimedOut() const {
            return stopWaitTimedOut_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool terminalTimedOut() const {
            return terminalTimedOut_.load(std::memory_order_acquire);
        }

      private:
        std::atomic_bool started_{false};
        std::atomic_bool finished_{false};
        std::atomic_bool stopObserved_{false};
        std::atomic_bool stopWaitTimedOut_{false};
        std::atomic_bool terminalTimedOut_{false};
        std::mutex mutex_;
        std::condition_variable_any condition_;
        bool release_{false};
    };

    class DelayedTerminalExportPort final : public ssa::ports::IExportPort {
      public:
        ssa::ports::WorkflowResult
        exportFilteredList(const ssa::ports::ExportFilteredListRequest&,
                           const std::stop_token stopToken = {}) override {
            started_.store(true, std::memory_order_release);
            while (!stopToken.stop_requested()) {
                std::this_thread::yield();
            }
            stopObserved_.store(true, std::memory_order_release);
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] { return release_; });
            return {ssa::ports::WorkflowStatus::Canceled, "Exportacao cancelada"};
        }

        void release() {
            {
                const std::scoped_lock lock(mutex_);
                release_ = true;
            }
            condition_.notify_all();
        }

        [[nodiscard]] bool started() const {
            return started_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool stopObserved() const {
            return stopObserved_.load(std::memory_order_acquire);
        }

      private:
        std::atomic_bool started_{false};
        std::atomic_bool stopObserved_{false};
        std::mutex mutex_;
        std::condition_variable condition_;
        bool release_{false};
    };

    class SlowCancelableRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit SlowCancelableRepository(const bool failAfterStop = false,
                                          const bool holdAfterStop = false)
            : failAfterStop_(failAfterStop), holdAfterStop_(holdAfterStop) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        const std::stop_token stopToken = {}) const override {
            if (request.searchText == "first") {
                firstStarted_.store(true, std::memory_order_release);
                while (!stopToken.stop_requested()) {
                    std::this_thread::yield();
                }
                if (holdAfterStop_) {
                    std::unique_lock lock(firstMutex_);
                    firstCondition_.wait(lock, [this] { return releaseFirst_; });
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds{300});
                }
                firstFinished_.store(true, std::memory_order_release);
                if (failAfterStop_) {
                    throw std::runtime_error("query failed after stop");
                }
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            secondStarted_.store(true, std::memory_order_release);
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

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {};
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

        void releaseFirst() {
            {
                const std::scoped_lock lock(firstMutex_);
                releaseFirst_ = true;
            }
            firstCondition_.notify_all();
        }

      private:
        bool failAfterStop_{false};
        bool holdAfterStop_{false};
        mutable std::atomic_bool firstStarted_{false};
        mutable std::atomic_bool firstFinished_{false};
        mutable std::atomic_bool secondStarted_{false};
        mutable std::mutex firstMutex_;
        mutable std::condition_variable firstCondition_;
        bool releaseFirst_{false};
    };

    class DetailsRelationRepository final : public ssa::ports::ISsaRepository {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber& number,
                          std::stop_token = {}) const override {
            const auto found = records_.find(number.value());
            if (found == records_.end()) {
                return std::nullopt;
            }
            return found->second;
        }

        std::vector<ssa::domain::SsaDerivadaEntry>
        derivadasDiretas(const ssa::domain::SsaNumber& number,
                         const std::stop_token stopToken = {}) const override {
            {
                std::unique_lock lock(childrenMutex_);
                if (heldChildrenNumber_ == number.value()) {
                    childrenStarted_.store(true, std::memory_order_release);
                    std::stop_callback stopCallback(stopToken,
                                                    [this] { childrenCondition_.notify_all(); });
                    childrenCondition_.wait(lock, [this, &stopToken] {
                        return childrenReleased_ || stopToken.stop_requested();
                    });
                    if (stopToken.stop_requested()) {
                        childrenCanceled_.store(true, std::memory_order_release);
                        throw std::system_error(
                            std::make_error_code(std::errc::operation_canceled));
                    }
                }
            }
            if (failedChildrenNumber_ == number.value()) {
                throw std::runtime_error("root children failed");
            }
            const auto found = children_.find(number.value());
            if (found == children_.end()) {
                return {};
            }
            return found->second;
        }

        std::vector<std::string> distinctValues(const ssa::domain::DistinctValuesRequest&,
                                                std::stop_token) const override {
            return {};
        }

        [[nodiscard]] std::size_t maxValueLength(std::string_view,
                                                 std::stop_token = {}) const override {
            return 0;
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest&,
                                          ssa::ports::SsaRecordConsumer,
                                          std::stop_token = {}) const override {
            return {0, {}};
        }

        void setRecord(std::string number, ssa::domain::SsaRecord record) {
            records_.insert_or_assign(std::move(number), std::move(record));
        }

        void setChildren(std::string number, std::vector<ssa::domain::SsaDerivadaEntry> children) {
            children_.insert_or_assign(std::move(number), std::move(children));
        }

        void holdChildrenFor(std::string number) {
            const std::scoped_lock lock(childrenMutex_);
            heldChildrenNumber_ = std::move(number);
            childrenReleased_ = false;
        }

        void failChildrenFor(std::string number) {
            failedChildrenNumber_ = std::move(number);
        }

        void releaseChildren() {
            {
                const std::scoped_lock lock(childrenMutex_);
                childrenReleased_ = true;
            }
            childrenCondition_.notify_all();
        }

        [[nodiscard]] bool childrenStarted() const {
            return childrenStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool childrenCanceled() const {
            return childrenCanceled_.load(std::memory_order_acquire);
        }

      private:
        std::map<std::string, ssa::domain::SsaRecord> records_;
        std::map<std::string, std::vector<ssa::domain::SsaDerivadaEntry>> children_;
        mutable std::mutex childrenMutex_;
        mutable std::condition_variable childrenCondition_;
        mutable std::atomic_bool childrenStarted_{false};
        mutable std::atomic_bool childrenCanceled_{false};
        std::string heldChildrenNumber_;
        std::string failedChildrenNumber_;
        bool childrenReleased_{false};
    };

    class SlowDetailsRepository final : public ssa::ports::ISsaRepository {
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
        derivadasDiretas(const ssa::domain::SsaNumber& number,
                         const std::stop_token stopToken = {}) const override {
            if (number.value() == "error") {
                throw std::runtime_error("relation query failed");
            }
            if (number.value() == "202500001") {
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
                return {{"old-child", "APV"}};
            }
            secondStarted_.store(true, std::memory_order_release);
            return {{"new-child", "SES"}};
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

        [[nodiscard]] bool firstStarted() const {
            return firstStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool firstFinished() const {
            return firstFinished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool secondStarted() const {
            return secondStarted_.load(std::memory_order_acquire);
        }

      private:
        mutable std::atomic_bool firstStarted_{false};
        mutable std::atomic_bool firstFinished_{false};
        mutable std::atomic_bool secondStarted_{false};
    };

    class SlowRelationNavigationRepository final : public ssa::ports::ISsaRepository {
      public:
        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request,
                                        std::stop_token = {}) const override {
            return {{}, 0, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&, std::stop_token = {}) const override {
            return 0;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber& number,
                          const std::stop_token stopToken = {}) const override {
            if (number.value() == "old") {
                firstStarted_.store(true, std::memory_order_release);
                const auto deadline =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds{500};
                while (!stopToken.stop_requested() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::yield();
                }
                if (stopToken.stop_requested()) {
                    stopObserved_.store(true, std::memory_order_release);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{200});
                firstFinished_.store(true, std::memory_order_release);
                if (stopToken.stop_requested()) {
                    throw std::system_error(std::make_error_code(std::errc::operation_canceled));
                }
            } else {
                secondStarted_.store(true, std::memory_order_release);
            }
            return ssa::domain::SsaRecord{{{"numero_ssa", number.value()}, {"situacao", "APV"}}};
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

        [[nodiscard]] bool firstStarted() const {
            return firstStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool firstFinished() const {
            return firstFinished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool secondStarted() const {
            return secondStarted_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool stopObserved() const {
            return stopObserved_.load(std::memory_order_acquire);
        }

      private:
        mutable std::atomic_bool firstStarted_{false};
        mutable std::atomic_bool firstFinished_{false};
        mutable std::atomic_bool secondStarted_{false};
        mutable std::atomic_bool stopObserved_{false};
    };

    class PresentationSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void date_text_formatter_keeps_only_day_month_year() {
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "2026-04-13 11:21:01", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "2026-04-13T12:26:00", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "sem data", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("sem data"));
        }

        void status_view_model_query_complete_messages_avoid_count_duplication() {
            ssa::presentation::StatusViewModel status;
            // Single page result: generic completion message, no count in the
            // message (count is shown separately in the status pill).
            status.setQueryComplete(10, 10, 1, 1);
            QCOMPARE(status.message(), QString("Consulta concluida"));
            QVERIFY(!status.message().contains("SSAs"));

            // Multipage: show page info instead of the count.
            status.setQueryComplete(10, 250, 1, 25);
            QCOMPARE(status.message(), QString("Pagina 1 de 25"));

            // Empty result.
            status.setQueryComplete(0, 100, 1, 10);
            QCOMPARE(status.message(), QString("Nenhum resultado"));
        }

        void recent_log_model_keeps_only_30_complete_copyable_events() {
            ssa::presentation::RecentLogModel logs;
            logs.append("Error", "Workflow", "Repeated", "Full diagnostic");
            logs.append("Error", "Workflow", "Repeated", "Full diagnostic");
            QCOMPARE(logs.rowCount(), 1);

            for (int index = 0; index < 31; ++index) {
                logs.append("Info", "Import", QStringLiteral("Event %1").arg(index),
                            QStringLiteral("Detail %1").arg(index));
            }

            QCOMPARE(logs.rowCount(), 30);
            QVERIFY(logs.entryText(0).contains("Event 30"));
            QVERIFY(logs.entryText(0).contains("Detail 30"));
            QVERIFY(!logs.allText().contains("Event 0\n"));
            QVERIFY(logs.allText().contains("Event 29"));
        }

        void load_populates_table_and_allows_details_selection() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QSignalSpy pageSpy(model.browse(), &ssa::presentation::BrowseViewModel::pageChanged);

            model.browse()->search()->setText("Teste");
            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QCOMPARE(model.browse()->totalRows(), 1);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));
            QVERIFY(pageSpy.size() >= 1);
            QCOMPARE(model.browse()->status()->loading(), false);
            QCOMPARE(model.browse()->tableModel()->columnLabel(0), QString("No SSA"));
            QVERIFY(model.browse()->tableModel()->columnWidth(0) > 0);
            QCOMPARE(model.browse()->tableModel()->ssaNumberAt(0), QString("202500001"));
            QCOMPARE(model.browse()->tableModel()->ssaNumberAt(99), QString());
        }

        void details_relation_current_carries_status_from_record() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);

            const auto relations = model.browse()->details()->relations();
            QVERIFY(!relations.empty());
            // The first relation is the Current node and must carry the
            // situacao status from the loaded record (FakeRepository sets APV).
            const auto current = relations.at(0).toMap();
            QCOMPARE(current.value("kind").toString(), QString("Atual"));
            QCOMPARE(current.value("status").toString(), QString("APV"));
            QCOMPARE(current.value("ssa").toString(), QString("202500001"));
        }

        void details_graph_model_rebuilds_after_load() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QVERIFY(model.browse()->details()->graphModel()->rowCount() == 0);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);

            // After load, the graph model must contain at least the target node.
            QVERIFY(model.browse()->details()->graphModel()->rowCount() >= 1);
            QCOMPARE(model.browse()->details()->graphModel()->target(), QString("202500001"));
        }

        void details_fields_keep_python_priority_order() {
            ssa::presentation::DetailsViewModel details;
            details.setRecord(ssa::domain::SsaRecord{{{"id", "3802"},
                                                      {"responsavel_execucao", "DANILO NADAL"},
                                                      {"descricao_ssa", "Descricao longa"},
                                                      {"numero_ssa", "202500003"},
                                                      {"qtd_derivadas", "2"},
                                                      {"situacao", "APV"},
                                                      {"solicitante", "CARLOS ORTIZ"},
                                                      {"localizacao_codigo", "T075Q002"},
                                                      {"setor_executor", "MEL4"},
                                                      {"responsavel_programacao", "DANILO NADAL"},
                                                      {"setor_emissor", "IEE3"}}});

            auto* fields = details.fields();
            QVERIFY(fields != nullptr);
            QCOMPARE(fields->rowCount(), 10);
            QCOMPARE(
                fields->data(fields->index(0, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("No SSA"));
            QCOMPARE(
                fields->data(fields->index(0, 0), ssa::presentation::DetailsFieldsModel::KeyRole)
                    .toString(),
                QString("numero_ssa"));
            QCOMPARE(
                fields->data(fields->index(1, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Sit."));
            QCOMPARE(
                fields->data(fields->index(2, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Loc."));
            QCOMPARE(
                fields->data(fields->index(3, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Emis."));
            QCOMPARE(
                fields->data(fields->index(4, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Exec."));
            QCOMPARE(
                fields->data(fields->index(5, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Qtd Der."));
            QCOMPARE(
                fields->data(fields->index(5, 0), ssa::presentation::DetailsFieldsModel::KeyRole)
                    .toString(),
                QString("qtd_derivadas"));
            QCOMPARE(
                fields->data(fields->index(8, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Resp. Programacao"));
            QCOMPARE(
                fields->data(fields->index(9, 0), ssa::presentation::DetailsFieldsModel::LabelRole)
                    .toString(),
                QString("Resp. Execucao"));
            for (int row = 0; row < fields->rowCount(); ++row) {
                QVERIFY(fields
                            ->data(fields->index(row, 0),
                                   ssa::presentation::DetailsFieldsModel::LabelRole)
                            .toString() != QString("ID"));
            }
        }

        void details_relation_navigation_indices_and_flags() {
            // FakeRepository returns nullopt for recordBySsaNumber, so
            // the async relation load will not change the record; but the index/flags
            // logic must still respond. Use a DetailsViewModel with a service.
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            // Build a record with an ancestor so relationCount >= 2.
            ssa::domain::SsaRecord record{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}};
            details.setRecord(record);

            QCOMPARE(details.relationCount(), 2);
            QCOMPARE(details.currentRelationIndex(), 1);
            QVERIFY(details.canSelectPreviousRelation());
            QVERIFY(!details.canSelectNextRelation());

            details.selectPreviousRelation();
            // FakeRepository returns nullopt so the record does not change,
            // but the navigation index advances.
            QCOMPARE(details.currentRelationIndex(), 0);
            QVERIFY(!details.canSelectPreviousRelation());
            QVERIFY(details.canSelectNextRelation());

            details.selectNextRelation();
            QCOMPARE(details.currentRelationIndex(), 1);
            QVERIFY(!details.canSelectNextRelation());

            // Out-of-range calls are no-ops.
            details.selectNextRelation();
            QCOMPARE(details.currentRelationIndex(), 1);
            details.selectPreviousRelation();
            details.selectPreviousRelation();
            QCOMPARE(details.currentRelationIndex(), 0);
        }

        void details_relation_navigation_preserves_the_table_selection_chain() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setRecord("202500001", ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                                                       {"situacao", "APV"}}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});

            QCOMPARE(details.relationCount(), 2);
            details.selectPreviousRelation();

            QTRY_COMPARE_WITH_TIMEOUT(details.selectedSsa(), QString("202500001"), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.relationCount(), 2);
            QCOMPARE(details.currentRelationIndex(), 0);
            QCOMPARE(details.graphModel()->target(), QString("202500003"));
            QVERIFY(details.canSelectNextRelation());
            QVERIFY(!details.canSelectPreviousRelation());
        }

        void details_relation_click_preserves_the_table_selection_chain() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setRecord("202500001", ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                                                       {"situacao", "APV"}}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});

            details.requestLoadRelationAt(0);

            QTRY_COMPARE_WITH_TIMEOUT(details.selectedSsa(), QString("202500001"), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.relationCount(), 2);
            QCOMPARE(details.currentRelationIndex(), 0);
            QCOMPARE(details.graphModel()->target(), QString("202500003"));
            QVERIFY(details.canSelectNextRelation());
            QVERIFY(!details.canSelectPreviousRelation());
        }

        void details_navigation_keeps_pending_root_children() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setRecord("202500001", ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                                                       {"situacao", "APV"}}});
            repository->setChildren("202500003", {{"202500004", "STE"}});
            repository->holdChildrenFor("202500003");
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});
            QTRY_VERIFY_WITH_TIMEOUT(repository->childrenStarted(), 1000);

            details.selectPreviousRelation();

            QTRY_COMPARE_WITH_TIMEOUT(details.selectedSsa(), QString("202500001"), 1000);
            repository->releaseChildren();
            QTRY_COMPARE_WITH_TIMEOUT(details.relationCount(), 3, 1000);
            QCOMPARE(details.relations().at(2).toMap().value("ssa").toString(),
                     QString("202500004"));
            QCOMPARE(details.currentRelationIndex(), 0);
            QCOMPARE(details.graphModel()->target(), QString("202500003"));
            QVERIFY(!repository->childrenCanceled());
        }

        void details_pending_root_children_keep_navigation_error() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setChildren("202500003", {{"202500004", "STE"}});
            repository->holdChildrenFor("202500003");
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});
            QTRY_VERIFY_WITH_TIMEOUT(repository->childrenStarted(), 1000);

            details.selectPreviousRelation();

            QTRY_COMPARE_WITH_TIMEOUT(details.relationError(), QString("SSA nao encontrada"), 1000);
            repository->releaseChildren();
            QTRY_COMPARE_WITH_TIMEOUT(details.relationCount(), 3, 1000);
            QCOMPARE(details.relationError(), QString("SSA nao encontrada"));
        }

        void details_navigation_success_keeps_root_children_error() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setRecord("202500001", ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                                                       {"situacao", "APV"}}});
            repository->failChildrenFor("202500003");
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});
            QTRY_COMPARE_WITH_TIMEOUT(details.relationError(), QString("root children failed"),
                                      1000);

            details.selectPreviousRelation();

            QTRY_COMPARE_WITH_TIMEOUT(details.selectedSsa(), QString("202500001"), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.relationError(), QString("root children failed"));
        }

        void details_relation_navigation_keeps_duplicate_relation_index() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setRecord("202500001", ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                                                       {"situacao", "APV"}}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{{{"numero_ssa", "202500003"},
                                                      {"situacao", "APV"},
                                                      {"derivada_de", "202500001"},
                                                      {"numero_ssa_relacionada_1", "202500001"}}});

            QCOMPARE(details.relationCount(), 3);
            QCOMPARE(details.currentRelationIndex(), 1);
            details.selectNextRelation();

            QTRY_COMPARE_WITH_TIMEOUT(details.selectedSsa(), QString("202500001"), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.currentRelationIndex(), 2);
            QCOMPARE(details.relationCount(), 3);
        }

        void details_relation_query_is_async_latest_wins_and_discards_stale_result() {
            auto repository = std::make_shared<SlowDetailsRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);
            QElapsedTimer elapsed;
            elapsed.start();

            details.setRecord(
                ssa::domain::SsaRecord{{{"numero_ssa", "202500001"}, {"situacao", "APV"}}});

            QVERIFY(elapsed.elapsed() < 100);
            QVERIFY(details.relationLoading());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            details.setRecord(
                ssa::domain::SsaRecord{{{"numero_ssa", "202500002"}, {"situacao", "APV"}}});

            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 1000);
            QVERIFY(!repository->firstFinished());
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.relationError(), QString());
            QCOMPARE(details.selectedSsaNumber(), QString("202500002"));
            QCOMPARE(details.relationCount(), 2);
            QCOMPARE(details.relations().at(1).toMap().value("ssa").toString(),
                     QString("new-child"));
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QCOMPARE(details.selectedSsaNumber(), QString("202500002"));
            QCOMPARE(details.relationCount(), 2);
        }

        void details_relation_query_exposes_failure_without_erasing_local_record() {
            auto repository = std::make_shared<SlowDetailsRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(
                ssa::domain::SsaRecord{{{"numero_ssa", "error"}, {"situacao", "APV"}}});

            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.relationError(), QString("relation query failed"));
            QCOMPARE(details.selectedSsaNumber(), QString("error"));
            QCOMPARE(details.relationCount(), 1);
        }

        void details_relation_navigation_load_is_async_latest_wins() {
            auto repository = std::make_shared<SlowRelationNavigationRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);
            QElapsedTimer elapsed;
            elapsed.start();

            details.requestLoadBySsaNumber(QStringLiteral("old"));

            QVERIFY(elapsed.elapsed() < 100);
            QVERIFY(details.relationLoading());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            details.requestLoadBySsaNumber(QStringLiteral("new"));

            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 1000);
            QVERIFY(!repository->firstFinished());
            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.selectedSsaNumber(), QString("new"));
            QCOMPARE(details.relationError(), QString());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QCOMPARE(details.selectedSsaNumber(), QString("new"));
        }

        void cancelAfterDetailsWorkerCompletedBeforeTerminalDoesNotPublishRecord() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setRecord("202500001", ssa::domain::SsaRecord{{{"numero_ssa", "202500001"},
                                                                       {"situacao", "APV"}}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.requestLoadBySsaNumber(QStringLiteral("202500001"));
            QVERIFY(QThreadPool::globalInstance()->waitForDone(1000));
            QVERIFY(details.relationLoading());

            details.cancel();

            QTRY_VERIFY_WITH_TIMEOUT(!details.relationLoading(), 1000);
            QCOMPARE(details.selectedSsaNumber(), QString());
            QCOMPARE(details.fieldCount(), 0);
        }

        void details_window_model_starts_loading_without_blocking_the_gui_thread() {
            auto repository = std::make_shared<SlowRelationNavigationRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QElapsedTimer elapsed;
            elapsed.start();

            auto* details = model.browse()->createDetailsWindowModel(QStringLiteral("old"), &model);

            QVERIFY(elapsed.elapsed() < 100);
            QVERIFY(details->relationLoading());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);
        }

        void search_apply_signal_reloads_table() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->search()->setText("bomba");
            model.browse()->search()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(QString::fromStdString(repository->requests().back().searchText),
                     QString("bomba"));
        }

        void details_navigation_walks_next_then_prev_within_page() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{3}, .rowCount = std::size_t{3}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QSignalSpy rowSpy(model.browse(),
                              &ssa::presentation::BrowseViewModel::currentRowChanged);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 3, 1000);

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));
            QVERIFY(model.browse()->canSelectNextRow());
            QVERIFY(!model.browse()->canSelectPreviousRow());
            QCOMPARE(rowSpy.size(), 1);

            model.browse()->selectNextRow();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 1, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500002"));
            QVERIFY(model.browse()->canSelectNextRow());
            QVERIFY(model.browse()->canSelectPreviousRow());

            model.browse()->selectNextRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 2, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500003"));
            QVERIFY(!model.browse()->canSelectNextRow());
            QVERIFY(model.browse()->canSelectPreviousRow());

            model.browse()->selectNextRow();
            QVERIFY(model.browse()->currentRow() == 2);

            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 1, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500002"));

            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));

            model.browse()->selectPreviousRow();
            QVERIFY(model.browse()->currentRow() == 0);
        }

        void selection_flow_open_ssa_dispatches_external_command() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QMetaObject::invokeMethod(model.selectionFlow(), "openSsa", Qt::QueuedConnection,
                                      Q_ARG(QString, QString("202500002")));
            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            QCOMPARE(commands->commands().front().kind, ssa::ports::ExternalCommandKind::OpenSsa);
            QCOMPARE(commands->commands().front().parameters.at("ssa_number"),
                     std::string("202500002"));
        }

        void selection_flow_open_ssa_ignores_empty_input() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QMetaObject::invokeMethod(model.selectionFlow(), "openSsa", Qt::QueuedConnection,
                                      Q_ARG(QString, QString("   ")));
            QTest::qWait(50);
            QCOMPARE(commands->commands().size(), std::size_t{0});
        }

        void derivadas_graph_model_builds_target_ancestor_and_child_layout() {
            ssa::presentation::DerivadasGraphModel model;
            QCOMPARE(model.nodeWidth(), 118.0);
            QCOMPARE(model.nodeHeight(), 48.0);
            QVariantList relations;
            relations.push_back(
                QVariantMap{{"kind", "Atual"}, {"ssa", "202500002"}, {"status", "APV"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});
            relations.push_back(
                QVariantMap{{"kind", "Relacionada"}, {"ssa", "202500003"}, {"status", "STE"}});

            QSignalSpy spy(&model, &ssa::presentation::DerivadasGraphModel::graphChanged);
            model.buildFromRelations(QStringLiteral("202500002"), relations);

            QCOMPARE(model.target(), QString("202500002"));
            QCOMPARE(model.rowCount(), 3);
            QCOMPARE(spy.size(), 1);
            QVERIFY(model.graphWidth() > 0);
            QVERIFY(model.graphHeight() > 0);

            // Ancestor first (depth 0), target second (depth 1), child third.
            QCOMPARE(model.nodeSsa(0), QString("202500001"));
            QCOMPARE(model.nodeIsTarget(0), false);
            QCOMPARE(model.nodeRole(0), QString("parent"));
            QCOMPARE(model.nodeSsa(1), QString("202500002"));
            QCOMPARE(model.nodeIsTarget(1), true);
            QCOMPARE(model.nodeRole(1), QString("current"));
            QCOMPARE(model.nodeStatus(1), QString("APV"));
            QCOMPARE(model.nodeSsa(2), QString("202500003"));
            QCOMPARE(model.nodeRole(2), QString("related"));
            QCOMPARE(model.nodeStatus(2), QString("STE"));

            const auto edges = model.edges();
            QCOMPARE(edges.size(), 2);
            // First edge: ancestor -> target (solid)
            QCOMPARE(edges.at(0).toMap().value("dashed"), false);
            // Second edge: target -> related (dashed)
            QCOMPARE(edges.at(1).toMap().value("dashed"), true);

            const auto svg = model.svg();
            QVERIFY(svg.contains(QStringLiteral("<svg")));
            QVERIFY(svg.contains(QStringLiteral("<path")));
            QVERIFY(svg.contains(QStringLiteral("202500002")));
            QVERIFY(svg.contains(QStringLiteral("APV")));
            QVERIFY(svg.contains(QStringLiteral("stroke-dasharray")));
        }

        void derivadas_graph_model_uses_details_view_model_relation_roles() {
            auto repository = std::make_shared<DetailsRelationRepository>();
            repository->setChildren("202500003", {{"202500004", "STE"}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::DetailsViewModel details(service);

            details.setRecord(ssa::domain::SsaRecord{
                {{"numero_ssa", "202500003"}, {"situacao", "APV"}, {"derivada_de", "202500001"}}});

            QTRY_COMPARE_WITH_TIMEOUT(details.relationCount(), 3, 1000);
            const auto relations = details.relations();
            QCOMPARE(relations.size(), 3);
            QCOMPARE(relations.at(0).toMap().value("kind").toString(), QString("Origem"));
            QCOMPARE(relations.at(0).toMap().value("role").toString(), QString("parent"));
            QCOMPARE(relations.at(1).toMap().value("kind").toString(), QString("Atual"));
            QCOMPARE(relations.at(1).toMap().value("role").toString(), QString("current"));
            QCOMPARE(relations.at(2).toMap().value("kind").toString(), QString("Derivada"));
            QCOMPARE(relations.at(2).toMap().value("role").toString(), QString("child"));

            const auto* graph = details.graphModel();
            QCOMPARE(graph->target(), QString("202500003"));
            QCOMPARE(graph->rowCount(), 3);
            QCOMPARE(graph->nodeSsa(0), QString("202500001"));
            QCOMPARE(graph->nodeIsTarget(0), false);
            QCOMPARE(graph->nodeRole(0), QString("parent"));
            QCOMPARE(graph->nodeSsa(1), QString("202500003"));
            QCOMPARE(graph->nodeIsTarget(1), true);
            QCOMPARE(graph->nodeRole(1), QString("current"));
            QCOMPARE(graph->nodeStatus(1), QString("APV"));
            QCOMPARE(graph->nodeSsa(2), QString("202500004"));
            QCOMPARE(graph->nodeRole(2), QString("child"));
            QCOMPARE(graph->nodeStatus(2), QString("STE"));
            QCOMPARE(graph->edges().size(), 2);
            QCOMPARE(graph->edges().at(0).toMap().value("dashed"), false);
            QCOMPARE(graph->edges().at(1).toMap().value("dashed"), false);
        }

        void derivadas_graph_model_clears_on_empty_target() {
            ssa::presentation::DerivadasGraphModel model;
            model.buildFromRelations(QStringLiteral("202500002"), {});
            QCOMPARE(model.rowCount(), 1);

            model.buildFromRelations(QString(), {});
            QCOMPARE(model.rowCount(), 0);
            QCOMPARE(model.target(), QString());
            QCOMPARE(model.graphWidth(), 0.0);
        }

        void derivadas_graph_model_layout_x_advances_with_depth() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"kind", "Atual"}, {"ssa", "202500003"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500002"}});
            relations.push_back(QVariantMap{{"kind", "Relacionada"}, {"ssa", "202500004"}});

            model.buildFromRelations(QStringLiteral("202500003"), relations);
            // 2 ancestors (depth 0,1) + target (depth 2) + 1 child (depth 3)
            QCOMPARE(model.rowCount(), 4);

            // x increases with depth: ancestor0 < ancestor1 < target < child
            const auto xAncestor0 = model.nodeCenter(0).x();
            const auto xAncestor1 = model.nodeCenter(1).x();
            const auto xTarget = model.nodeCenter(2).x();
            const auto xChild = model.nodeCenter(3).x();
            QVERIFY(xAncestor0 < xAncestor1);
            QVERIFY(xAncestor1 < xTarget);
            QVERIFY(xTarget < xChild);

            // ancestors and target stay on the same row; child relations are one level below.
            QCOMPARE(model.nodeCenter(0).y(), model.nodeCenter(1).y());
            QCOMPARE(model.nodeCenter(1).y(), model.nodeCenter(2).y());
            QVERIFY(model.nodeCenter(3).y() > model.nodeCenter(2).y());
        }

        void derivadas_graph_model_rotates_and_centers_for_narrow_viewport() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"kind", "Atual"}, {"ssa", "202500003"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500002"}});
            relations.push_back(QVariantMap{{"kind", "Relacionada"}, {"ssa", "202500004"}});

            model.buildFromRelations(QStringLiteral("202500003"), relations);
            QCOMPARE(model.orientation(), QString("horizontal"));

            model.setViewportSize(640, 480);
            QCOMPARE(model.orientation(), QString("vertical"));
            qreal minLeft = model.graphWidth();
            qreal minTop = model.graphHeight();
            qreal maxRight = 0;
            qreal maxBottom = 0;
            for (int row = 0; row < model.rowCount(); ++row) {
                const auto center = model.nodeCenter(row);
                minLeft = std::min(minLeft, center.x() - model.nodeWidth() / 2.0);
                minTop = std::min(minTop, center.y() - model.nodeHeight() / 2.0);
                maxRight = std::max(maxRight, center.x() + model.nodeWidth() / 2.0);
                maxBottom = std::max(maxBottom, center.y() + model.nodeHeight() / 2.0);
            }
            QCOMPARE(minLeft, 8.0);
            QCOMPARE(minTop, 8.0);
            QCOMPARE(model.graphWidth() - maxRight, 8.0);
            QCOMPARE(model.graphHeight() - maxBottom, 8.0);

            for (const auto& edgeValue : model.edges()) {
                const auto edge = edgeValue.toMap();
                QVERIFY(edge.contains("routeY"));
                QVERIFY(edge.value("fromY").toReal() < edge.value("toY").toReal());
            }
        }

        void derivadas_graph_model_remembers_empty_narrow_viewport() {
            ssa::presentation::DerivadasGraphModel model;
            model.setViewportSize(640, 480);

            QVariantList relations;
            relations.push_back(QVariantMap{{"role", "current"}, {"ssa", "202500100"}});
            relations.push_back(QVariantMap{{"role", "child"}, {"ssa", "202500101"}});
            model.buildFromRelations(QStringLiteral("202500100"), relations);

            QCOMPARE(model.orientation(), QString("vertical"));
            QVERIFY(model.nodeCenter(1).y() > model.nodeCenter(0).y());
        }

        void derivadas_graph_model_fans_many_children_below_target() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"role", "current"}, {"ssa", "202500100"}});
            for (int index = 0; index < 10; ++index) {
                relations.push_back(QVariantMap{
                    {"role", "child"}, {"ssa", QStringLiteral("20250010%1").arg(index + 1)}});
            }

            model.buildFromRelations(QStringLiteral("202500100"), relations);

            QCOMPARE(model.rowCount(), 11);
            const auto targetY = model.nodeCenter(0).y();
            const auto targetX = model.nodeCenter(0).x();
            for (int row = 1; row < model.rowCount(); ++row) {
                QVERIFY(model.nodeCenter(row).y() > targetY);
                QVERIFY(model.nodeCenter(row).x() > targetX);
            }
            QVERIFY(model.nodeCenter(1).x() < model.nodeCenter(4).x());
            QVERIFY(model.nodeCenter(5).x() == model.nodeCenter(1).x());
            QVERIFY(model.nodeCenter(5).y() > model.nodeCenter(1).y());

            const auto edges = model.edges();
            QCOMPARE(edges.size(), 10);
            for (const auto& edgeValue : edges) {
                const auto edge = edgeValue.toMap();
                QCOMPARE(edge.value("from").toString(), QString("202500100"));
                QVERIFY(edge.value("to").toString().startsWith(QString("20250010")));
                QVERIFY(edge.value("fromX").toReal() > targetX);
                QVERIFY(edge.value("toX").toReal() > edge.value("fromX").toReal());
                QVERIFY(edge.value("toY").toReal() > edge.value("fromY").toReal());
                QVERIFY(edge.value("routeX").toReal() > edge.value("fromX").toReal());
                QVERIFY(edge.value("routeX").toReal() < edge.value("toX").toReal());
            }
        }

        void derivadas_graph_model_routes_second_row_between_node_rows() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"role", "current"}, {"ssa", "202500100"}});
            for (int index = 0; index < 8; ++index) {
                relations.push_back(QVariantMap{
                    {"role", "child"},
                    {"ssa", QStringLiteral("2025001%1").arg(index + 1, 2, 10, QChar('0'))}});
            }

            model.setViewportSize(640, 480);
            model.buildFromRelations(QStringLiteral("202500100"), relations);

            const auto firstRowBottom = model.nodeCenter(1).y() + model.nodeHeight() / 2.0;
            const auto secondRowTop = model.nodeCenter(5).y() - model.nodeHeight() / 2.0;
            const auto edges = model.edges();
            QCOMPARE(edges.size(), 8);
            for (int index = 4; index < edges.size(); ++index) {
                const auto routeY = edges.at(index).toMap().value("routeY").toReal();
                QVERIFY(routeY > firstRowBottom);
                QVERIFY(routeY < secondRowTop);
            }
        }

        void derivadas_graph_model_exposes_shared_route_junctions() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"role", "current"}, {"ssa", "202500100"}});
            for (int index = 0; index < 8; ++index) {
                relations.push_back(QVariantMap{
                    {"role", "child"},
                    {"ssa", QStringLiteral("2025001%1").arg(index + 1, 2, 10, QChar('0'))}});
            }

            model.setViewportSize(640, 480);
            model.buildFromRelations(QStringLiteral("202500100"), relations);

            const auto junctions = model.junctions();
            QCOMPARE(junctions.size(), 2);
            for (const auto& junctionValue : junctions) {
                const auto junction = junctionValue.toMap();
                QCOMPARE(junction.value("branchCount").toInt(), 4);
                QVERIFY(junction.value("x").toReal() >= 0.0);
                QVERIFY(junction.value("y").toReal() >= 0.0);
            }
            QCOMPARE(model.svg().count(QStringLiteral("class=\"junction\"")), 2);
        }

        void derivadas_graph_model_is_deterministic_bounded_and_non_overlapping() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"role", "current"}, {"ssa", "202500500"}});
            for (int index = 0; index < 3; ++index) {
                relations.push_back(QVariantMap{{"role", "parent"},
                                                {"ssa", QStringLiteral("20250040%1").arg(index)}});
            }
            for (int index = 0; index < 21; ++index) {
                relations.push_back(QVariantMap{
                    {"role", "child"},
                    {"ssa", QStringLiteral("2025006%1").arg(index, 2, 10, QChar('0'))}});
            }

            const auto verifyLayout = [&](const QSizeF viewport, const QString& orientation) {
                model.buildFromRelations(QStringLiteral("202500500"), relations);
                model.setViewportSize(viewport.width(), viewport.height());
                QCOMPARE(model.orientation(), orientation);
                QCOMPARE(model.rowCount(), 25);

                QList<QPointF> firstLayout;
                QList<QRectF> nodeBounds;
                for (int row = 0; row < model.rowCount(); ++row) {
                    const auto center = model.nodeCenter(row);
                    firstLayout.push_back(center);
                    const QRectF bounds{center.x() - model.nodeWidth() / 2.0,
                                        center.y() - model.nodeHeight() / 2.0, model.nodeWidth(),
                                        model.nodeHeight()};
                    nodeBounds.push_back(bounds);
                    QVERIFY(bounds.left() >= 0);
                    QVERIFY(bounds.top() >= 0);
                    QVERIFY(bounds.right() <= model.graphWidth());
                    QVERIFY(bounds.bottom() <= model.graphHeight());
                }
                for (qsizetype left = 0; left < nodeBounds.size(); ++left) {
                    for (qsizetype right = left + 1; right < nodeBounds.size(); ++right) {
                        QVERIFY(!nodeBounds[left].intersects(nodeBounds[right]));
                    }
                }

                const auto edges = model.edges();
                QCOMPARE(edges.size(), 24);
                for (const auto& edgeValue : edges) {
                    const auto edge = edgeValue.toMap();
                    for (const auto* key : {"fromX", "toX"}) {
                        const auto coordinate = edge.value(QLatin1StringView{key}).toReal();
                        QVERIFY(coordinate >= 0);
                        QVERIFY(coordinate <= model.graphWidth());
                    }
                    for (const auto* key : {"fromY", "toY"}) {
                        const auto coordinate = edge.value(QLatin1StringView{key}).toReal();
                        QVERIFY(coordinate >= 0);
                        QVERIFY(coordinate <= model.graphHeight());
                    }
                    const auto routeKey = orientation == QStringLiteral("vertical")
                                              ? QStringLiteral("routeY")
                                              : QStringLiteral("routeX");
                    const auto routeLimit = orientation == QStringLiteral("vertical")
                                                ? model.graphHeight()
                                                : model.graphWidth();
                    QVERIFY(edge.contains(routeKey));
                    const auto routeCoordinate = edge.value(routeKey).toReal();
                    QVERIFY(routeCoordinate >= 0);
                    QVERIFY(routeCoordinate <= routeLimit);
                }

                const auto firstSvg = model.svg();

                model.buildFromRelations(QStringLiteral("202500500"), relations);
                model.setViewportSize(viewport.width(), viewport.height());
                QCOMPARE(model.svg(), firstSvg);
                for (int row = 0; row < model.rowCount(); ++row) {
                    QCOMPARE(model.nodeCenter(row), firstLayout[row]);
                }
            };

            verifyLayout(QSizeF{1'400, 700}, QStringLiteral("horizontal"));
            verifyLayout(QSizeF{640, 900}, QStringLiteral("vertical"));
        }

        void derivadas_graph_model_invalid_index_returns_empty() {
            ssa::presentation::DerivadasGraphModel model;
            model.buildFromRelations(QStringLiteral("202500001"),
                                     {QVariantMap{{"kind", "Atual"}, {"ssa", "202500001"}}});

            QCOMPARE(model.nodeSsa(-1), QString());
            QCOMPARE(model.nodeStatus(-1), QString());
            QCOMPARE(model.nodeSsa(99), QString());
            QCOMPARE(model.nodeStatus(99), QString());
            QCOMPARE(model.nodeIsTarget(-1), false);
            QVERIFY(model.nodeCenter(-1).isNull());
        }

        void derivadas_graph_model_dedupes_repeated_relations() {
            ssa::presentation::DerivadasGraphModel model;
            QVariantList relations;
            relations.push_back(QVariantMap{{"kind", "Atual"}, {"ssa", "202500002"}});
            // Same ancestor twice
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});
            relations.push_back(QVariantMap{{"kind", "Derivada de"}, {"ssa", "202500001"}});

            model.buildFromRelations(QStringLiteral("202500002"), relations);
            // Target + 1 deduped ancestor = 2 nodes (not 3)
            QCOMPARE(model.rowCount(), 2);
            QCOMPARE(model.edges().size(), 1);
        }

        void derivadas_graph_model_ignores_target_repeated_as_a_relation() {
            ssa::presentation::DerivadasGraphModel model;
            model.buildFromRelations(
                QStringLiteral("202500002"),
                {QVariantMap{{"role", "parent"}, {"ssa", "202500002"}},
                 QVariantMap{{"role", "current"}, {"ssa", "202500002"}, {"status", "APV"}},
                 QVariantMap{{"role", "child"}, {"ssa", "202500002"}},
                 QVariantMap{{"role", "related"}, {"ssa", "202500002"}}});

            QCOMPARE(model.rowCount(), 1);
            QCOMPARE(model.nodeSsa(0), QStringLiteral("202500002"));
            QCOMPARE(model.nodeRole(0), QStringLiteral("current"));
            QCOMPARE(model.nodeStatus(0), QStringLiteral("APV"));
            QCOMPARE(model.edges().size(), 0);
        }

        void details_navigation_walks_across_pages_next_then_prev() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}, .rowCount = std::size_t{10}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->setPageSize(10);
            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 10, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->pageCount(), 3);

            // Walk to last row of page 1
            for (int i = 0; i < 9; ++i) {
                model.browse()->selectNextRow();
            }
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 9, 1000);
            QVERIFY(model.browse()->canSelectNextRow());
            QCOMPARE(model.browse()->pageNumber(), 1);

            // Cross to page 2 -> auto-select first row
            model.browse()->selectNextRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));

            // Previous from first row of page 2 -> back to page 1 last row
            QVERIFY(model.browse()->canSelectPreviousRow());
            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 1, 2000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 9, 2000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500010"));
        }

        void sort_by_column_updates_request_contract() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.browse()->sortByColumn(1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);

            const auto requests = repository->requests();
            QCOMPARE(QString::fromStdString(requests.back().sort.columnKey), QString("situacao"));
            QCOMPARE(requests.back().sort.ascending, true);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
        }

        void sort_cycle_ascends_descends_then_clears() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), false);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString(""));
            QCOMPARE(model.browse()->sortAscending(), false);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);
        }

        void reset_sort_clears_sorting_and_reloads_default_order() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.browse()->sortByColumn(1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);

            model.browse()->resetSort();

            QCOMPARE(model.browse()->sortColumnKey(), QString(""));
            QCOMPARE(model.browse()->sortAscending(), false);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{3}, 1000);
            QCOMPARE(repository->requests().back().sort.columnKey, std::string{});
            QCOMPARE(repository->requests().back().sort.ascending, false);
        }

        void sort_by_column_resets_page_and_saves_preferences() {
            const auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{21}});
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);
            const auto hasPageWithSort = [&repository] {
                const auto requests = repository->requests();
                return std::any_of(requests.begin(), requests.end(), [](const auto& request) {
                    return request.pageIndex == 0 && request.sort.columnKey == "situacao";
                });
            };

            model.browse()->setPageSize(10);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageCount(), 3, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(
                [&repository] {
                    const auto requests = repository->requests();
                    return std::any_of(requests.begin(), requests.end(),
                                       [](const auto& request) { return request.pageIndex == 1; });
                }(),
                1000);

            model.browse()->sortByColumn(1);

            QTRY_VERIFY_WITH_TIMEOUT(hasPageWithSort(), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 1, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(QString::fromStdString(preferences->snapshot().sortColumnKey),
                                      QString("situacao"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().sortAscending, true, 1000);
        }

        void table_headers_expose_sort_indicator_state() {
            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.browse()->sortByColumn(1);

            const auto headers = model.browse()->tableHeaders();
            QVERIFY(headers.size() > 1);
            const auto sortedHeader = headers[1].toMap();
            QCOMPARE(sortedHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(sortedHeader.value("sorted").toBool(), true);
            QCOMPARE(sortedHeader.value("sortAscending").toBool(), true);

            const auto unsortedHeader = headers[0].toMap();
            QCOMPARE(unsortedHeader.value("sorted").toBool(), false);

            model.browse()->filters()->setColumnFilters({{"situacao", "APV"}});
            const auto filteredHeaders = model.browse()->tableHeaders();
            const auto filteredHeader = filteredHeaders[1].toMap();
            QCOMPARE(filteredHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(filteredHeader.value("filtered").toBool(), true);
            const auto unfilteredHeader = filteredHeaders[0].toMap();
            QCOMPARE(unfilteredHeader.value("filtered").toBool(), false);

            model.browse()->sortByColumn(1);
            const auto descendingHeaders = model.browse()->tableHeaders();
            const auto descendingHeader = descendingHeaders[1].toMap();
            QCOMPARE(descendingHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(descendingHeader.value("sorted").toBool(), true);
            QCOMPARE(descendingHeader.value("sortAscending").toBool(), false);
        }

        void next_page_reaches_final_page() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{21}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            const auto hasPage = [&repository](const std::size_t pageIndex) {
                const auto requests = repository->requests();
                return std::any_of(
                    requests.begin(), requests.end(),
                    [pageIndex](const auto& request) { return request.pageIndex == pageIndex; });
            };

            model.browse()->setPageSize(10);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageCount(), 3, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(hasPage(1), 3000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 3, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(hasPage(2), 3000);

            QCOMPARE(model.browse()->pageNumber(), 3);
            QCOMPARE(model.browse()->pageCount(), 3);
        }

        void cancel_marks_current_request_as_stale() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.delay = std::chrono::milliseconds{80}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->search()->setText("Primeira");
            model.browse()->apply();
            model.browse()->cancelCurrentRequest();

            QTest::qWait(160);
            QCOMPARE(model.browse()->tableModel()->rowCount(), 0);
            QCOMPARE(model.browse()->status()->message(), QString("Consulta cancelada"));
        }

        void cancelAfterPageWorkerCompletedBeforeTerminalDoesNotPublishSuccess() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            QSignalSpy succeededSpy(&coordinator,
                                    &ssa::presentation::PageQueryCoordinator::succeeded);
            QSignalSpy canceledSpy(&coordinator,
                                   &ssa::presentation::PageQueryCoordinator::canceled);

            coordinator.run({});
            QVERIFY(QThreadPool::globalInstance()->waitForDone(1000));
            QCOMPARE(coordinator.state(), ssa::presentation::PageQueryCoordinator::State::Running);

            coordinator.cancel();

            QCOMPARE(coordinator.state(),
                     ssa::presentation::PageQueryCoordinator::State::Canceling);
            QTRY_COMPARE_WITH_TIMEOUT(coordinator.state(),
                                      ssa::presentation::PageQueryCoordinator::State::Idle, 1000);
            QCOMPARE(succeededSpy.size(), 0);
            QCOMPARE(canceledSpy.size(), 1);
        }

        void page_query_starts_latest_request_before_canceled_worker_finishes() {
            auto repository = std::make_shared<SlowCancelableRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            int canceledCount = 0;
            int failedCount = 0;
            std::string completedSearch;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                    [&](const ssa::presentation::PageQueryResult&,
                        const ssa::domain::SsaPageRequest& request) {
                        ++succeededCount;
                        completedSearch = request.searchText;
                    });
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, this,
                    [&] { ++canceledCount; });
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::failed, this,
                    [&](const QString&) { ++failedCount; });
            ssa::domain::SsaPageRequest firstRequest;
            firstRequest.searchText = "first";

            coordinator.run(firstRequest);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            auto secondRequest = firstRequest;
            secondRequest.searchText = "second";
            coordinator.run(secondRequest);

            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 150);
            QVERIFY(!repository->firstFinished());
            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QCOMPARE(completedSearch, std::string{"second"});
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QCoreApplication::processEvents();
            QCOMPARE(succeededCount, 1);
            QCOMPARE(canceledCount, 0);
            QCOMPARE(failedCount, 0);
        }

        void page_query_prefetches_two_pages_and_reuses_the_cache() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            QSignalSpy startedSpy(&coordinator, &ssa::presentation::PageQueryCoordinator::started);
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                    [&](const ssa::presentation::PageQueryResult&,
                        const ssa::domain::SsaPageRequest&) { ++succeededCount; });

            ssa::domain::SsaPageRequest first;
            first.pageSize = 10;
            coordinator.run(first);

            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QCOMPARE(startedSpy.size(), 1);
            const auto prefetchedRequests = repository->requests();
            QCOMPARE(prefetchedRequests.size(), std::size_t{3});
            std::vector<std::size_t> prefetchedPageIndices;
            prefetchedPageIndices.reserve(prefetchedRequests.size());
            for (const auto& request : prefetchedRequests) {
                prefetchedPageIndices.push_back(request.pageIndex);
            }
            std::ranges::sort(prefetchedPageIndices);
            QCOMPARE(prefetchedPageIndices, std::vector<std::size_t>({0, 1, 2}));

            auto second = first;
            second.pageIndex = 1;
            coordinator.run(second);

            QCOMPARE(succeededCount, 2);
            QCOMPARE(repository->requests().size(), std::size_t{3});

            coordinator.run(second);

            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 3, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QCOMPARE(repository->requests().size(), std::size_t{3});
        }

        void page_query_fingerprint_change_does_not_reuse_stale_prefetch() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                    [&](const ssa::presentation::PageQueryResult&,
                        const ssa::domain::SsaPageRequest&) { ++succeededCount; });

            ssa::domain::SsaPageRequest initial;
            initial.pageSize = 10;
            coordinator.run(initial);
            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QCOMPARE(repository->requests().size(), std::size_t{3});

            auto changed = initial;
            changed.pageIndex = 1;
            changed.searchText = "different fingerprint";
            coordinator.run(changed);

            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 2, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QVERIFY(repository->requests().size() > std::size_t{3});
            QCOMPARE(repository->requests().at(3), changed);
        }

        void page_query_generation_invalidation_does_not_reuse_stale_prefetch() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                    [&](const ssa::presentation::PageQueryResult&,
                        const ssa::domain::SsaPageRequest&) { ++succeededCount; });

            ssa::domain::SsaPageRequest initial;
            initial.pageSize = 10;
            coordinator.run(initial);
            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QCOMPARE(repository->requests().size(), std::size_t{3});

            coordinator.invalidateTotalRowsAll();
            auto secondPage = initial;
            secondPage.pageIndex = 1;
            coordinator.run(secondPage);

            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 2, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QVERIFY(repository->requests().size() > std::size_t{3});
            QCOMPARE(repository->requests().at(3), secondPage);
        }

        void page_query_cancel_during_success_suppresses_prefetch() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            bool successSeen = false;
            connect(
                &coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                [&](const ssa::presentation::PageQueryResult&, const ssa::domain::SsaPageRequest&) {
                    successSeen = true;
                    coordinator.cancel();
                });

            coordinator.run({});

            QTRY_VERIFY_WITH_TIMEOUT(successSeen, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QCOMPARE(repository->requests().size(), std::size_t{1});
        }

        void page_query_cache_hit_blocks_reentrant_request() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            bool reenterOnStart = false;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                    [&](const ssa::presentation::PageQueryResult&,
                        const ssa::domain::SsaPageRequest&) { ++succeededCount; });
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::started, this, [&] {
                if (reenterOnStart) {
                    auto newerRequest = ssa::domain::SsaPageRequest{};
                    newerRequest.searchText = "reentrant";
                    coordinator.run(newerRequest);
                }
            });

            ssa::domain::SsaPageRequest first;
            first.pageSize = 10;
            coordinator.run(first);
            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);

            auto cached = first;
            cached.pageIndex = 1;
            reenterOnStart = true;
            coordinator.run(cached);

            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 2, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
            QCOMPARE(repository->requests().size(), std::size_t{3});
        }

        void invalidating_totals_restarts_same_inflight_query() {
            auto repository = std::make_shared<FakeRepository>(FakeRepositoryConfig{
                .delay = std::chrono::milliseconds{150}, .totalRows = std::size_t{25}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            ssa::domain::SsaPageRequest request;

            coordinator.run(request);
            QTRY_COMPARE_WITH_TIMEOUT(repository->startedRequests().size(), std::size_t{1}, 500);

            coordinator.invalidateTotalRowsAll();
            coordinator.run(request);

            QTRY_VERIFY_WITH_TIMEOUT(repository->startedRequests().size() >= std::size_t{2}, 500);
            coordinator.cancel();
            QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hasActiveOperations(), 1000);
        }

        void page_query_cancel_is_terminal_and_blocks_new_work() {
            auto repository = std::make_shared<SlowCancelableRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int canceledCount = 0;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, this,
                    [&] { ++canceledCount; });
            ssa::domain::SsaPageRequest firstRequest;
            firstRequest.searchText = "first";

            coordinator.run(firstRequest);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            QElapsedTimer elapsed;
            elapsed.start();
            coordinator.cancel();

            QVERIFY(elapsed.elapsed() < 50);
            QCOMPARE(coordinator.state(),
                     ssa::presentation::PageQueryCoordinator::State::Canceling);
            QCOMPARE(canceledCount, 0);

            auto secondRequest = firstRequest;
            secondRequest.searchText = "second";
            coordinator.run(secondRequest);
            QTest::qWait(50);
            QVERIFY(!repository->secondStarted());
            QCOMPARE(canceledCount, 0);

            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(canceledCount, 1, 1000);
            QCOMPARE(coordinator.state(), ssa::presentation::PageQueryCoordinator::State::Idle);
        }

        void page_query_cancel_logs_worker_failure_and_stays_canceled() {
            auto repository = std::make_shared<SlowCancelableRepository>(true);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int canceledCount = 0;
            QString failure;
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::canceled, this,
                    [&] { ++canceledCount; });
            connect(&coordinator, &ssa::presentation::PageQueryCoordinator::failed, this,
                    [&](const QString& message) { failure = message; });
            ssa::domain::SsaPageRequest request;
            request.searchText = "first";

            coordinator.run(request);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);
            QTest::ignoreMessage(QtWarningMsg,
                                 "Page query failed after cancellation: query failed after stop");
            coordinator.cancel();

            QTRY_COMPARE_WITH_TIMEOUT(canceledCount, 1, 1000);
            QVERIFY(failure.isEmpty());
            QCOMPARE(coordinator.state(), ssa::presentation::PageQueryCoordinator::State::Idle);
        }

        void page_query_terminal_slot_cannot_start_reentrant_work() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::PageQueryCoordinator coordinator(service);
            int succeededCount = 0;
            connect(
                &coordinator, &ssa::presentation::PageQueryCoordinator::succeeded, this,
                [&](const ssa::presentation::PageQueryResult&, const ssa::domain::SsaPageRequest&) {
                    ++succeededCount;
                    if (succeededCount == 1) {
                        ssa::domain::SsaPageRequest secondRequest;
                        secondRequest.searchText = "second";
                        coordinator.run(secondRequest);
                    }
                });

            coordinator.run({});

            QTRY_COMPARE_WITH_TIMEOUT(succeededCount, 1, 1000);
            QTest::qWait(50);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(coordinator.state(), ssa::presentation::PageQueryCoordinator::State::Idle);
        }

        void column_settings_update_visible_columns_and_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 180));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(request.visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(request.visibleColumns.front()), QString("numero_ssa"));
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 180,
                                      1000);
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 180);
        }

        void column_settings_allows_wide_description_column() {
            ssa::presentation::ColumnSettingsModel columns;

            QVERIFY(columns.setColumnWidth(QStringLiteral("descricao_ssa"), 900));

            QCOMPARE(columns.maxColumnWidth(), 2400);
            QCOMPARE(columns.columnWidths().at("descricao_ssa"), 900);
        }

        void column_display_catalog_pairs_default_widths_by_key() {
            const ssa::presentation::SsaColumnDisplayCatalog catalog;
            const auto columns = catalog.all();

            QCOMPARE(columns.size(), ssa::domain::ColumnCatalog::all().size());
            for (const auto& column : columns) {
                QVERIFY(column.defaultWidth > 0);
            }
            QCOMPARE(catalog.resolve("numero_ssa").defaultWidth, 98);
            QCOMPARE(catalog.resolve("situacao").defaultWidth, 60);
            QCOMPARE(catalog.resolve("descricao_ssa").defaultWidth, 640);
            QCOMPARE(catalog.resolve("data_cadastro").defaultWidth, 100);
        }

        void column_width_update_does_not_reload_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 220);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 220,
                                      1000);
        }

        void apply_column_settings_persists_applied_snapshot_not_later_staging() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 300));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{2},
                                      1000);
            const auto saved = preferences->snapshot();
            QCOMPARE(QString::fromStdString(saved.visibleColumns.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(saved.visibleColumns.at(1)), QString("situacao"));
            QCOMPARE(saved.columnWidths.at("numero_ssa"), 220);
        }

        void column_width_flow_persists_without_reloading_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnWidthAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(int, 230));

            QCOMPARE(changed, true);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 230);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 230,
                                      1000);
        }

        void immediate_column_width_flow_ignores_unapplied_popup_edits() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 230));
            QVERIFY(model.columns()->setColumnWidth("situacao", 260));

            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnWidthAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(int, 230));

            QCOMPARE(changed, true);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 230,
                                      1000);
            QCOMPARE(preferences->snapshot().columnWidths.at("situacao"), 160);
            QCOMPARE(preferences->snapshot().visibleColumns.size(), std::size_t{3});
            QCOMPARE(QString::fromStdString(preferences->snapshot().visibleColumns.at(2)),
                     QString("setor_executor"));
        }

        void column_visibility_flow_persists_and_reloads_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "situacao"),
                                      Q_ARG(bool, false));

            QCOMPARE(changed, true);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QCOMPARE(repository->requests().back().visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(repository->requests().back().visibleColumns.front()),
                     QString("numero_ssa"));
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{1},
                                      1000);
        }

        void immediate_column_visibility_flow_ignores_unapplied_popup_edits() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 230));

            bool canHideStagedColumn = false;
            QMetaObject::invokeMethod(model.columnFlow(), "canHideColumn",
                                      qReturnArg(canHideStagedColumn),
                                      Q_ARG(QString, "setor_executor"));
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "situacao"),
                                      Q_ARG(bool, false));

            QCOMPARE(canHideStagedColumn, true);
            QCOMPARE(changed, true);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{2},
                                      1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                QString::fromStdString(preferences->snapshot().visibleColumns.at(0)),
                QString("numero_ssa"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                QString::fromStdString(preferences->snapshot().visibleColumns.at(1)),
                QString("setor_executor"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 140,
                                      1000);
            const auto stagedVisibleKeys = model.columns()->visibleKeys();
            QCOMPARE(stagedVisibleKeys.size(), std::size_t{2});
            QCOMPARE(QString::fromStdString(stagedVisibleKeys.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(stagedVisibleKeys.at(1)), QString("setor_executor"));
        }

        void column_visibility_flow_keeps_one_visible_column() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa"};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            bool canHide = true;
            QMetaObject::invokeMethod(model.columnFlow(), "canHideColumn", qReturnArg(canHide),
                                      Q_ARG(QString, "numero_ssa"));
            bool changed = true;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(bool, false));

            QCOMPARE(canHide, false);
            QCOMPARE(changed, false);
            QCOMPARE(model.columns()->visibleKeys().size(), std::size_t{1});
            QCOMPARE(repository->requests().size(), std::size_t{0});
            QCOMPARE(preferences->snapshot().visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(preferences->snapshot().visibleColumns.front()),
                     QString("numero_ssa"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void theme_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setTheme("tokyo-night");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->theme(), QString("tokyo-night"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().theme), QString("tokyo-night"));
        }

        void sam_refresh_preferences_are_loaded_and_saved() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.samRefresh.enabled = true;
            initial.samRefresh.intervalMinutes = 45;
            initial.samRefresh.baseUrl = "https://apps.example.test/SAM/rest";
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QCOMPARE(model.actions()->workflows()->samRefreshEnabled(), true);
            QCOMPARE(model.actions()->workflows()->samIntervalMinutes(), 45);
            QCOMPARE(model.actions()->workflows()->samBaseUrl(),
                     QStringLiteral("https://apps.example.test/SAM/rest"));

            model.actions()->workflows()->setSamIntervalMinutes(90);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().samRefresh.intervalMinutes, 90, 1000);
            QCOMPARE(preferences->snapshot().samRefresh.enabled, true);
        }

        void pyqt_theme_catalog_is_accepted() {
            const QStringList themes{
                "grayscale",
                "windows7",
                "classico",
                "gruvbox",
                "dark",
                "dracula",
                "solarized-dark",
                "solarized-light",
                "mint-light",
                "paper",
                "tokyo-night",
                "catppuccin",
                "nord",
                "ssa-dark",
                "ayu-light",
                "ayu-mirage",
                "flexoki-dark",
                "flexoki-light",
                "kanagawa",
                "kanagawa-dragon",
                "rose-pine",
                "rose-pine-moon",
                "rose-pine-dawn",
                "primer-dark",
                "primer-light",
                "oxocarbon-light",
                "grayscalepy",
                "windows7py",
                "classicopy",
                "gruvboxpy",
                "darkpy",
                "draculapy",
                "solarized-darkpy",
                "solarized-lightpy",
                "mint-lightpy",
                "paperpy",
                "tokyo-nightpy",
                "catppuccinpy",
                "nordpy",
            };

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            for (const QString& theme : themes) {
                model.ui()->setTheme(theme);
                QCOMPARE(model.ui()->theme(), theme);
            }
        }

        void theme_can_be_reverted_after_preview_change() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            const QString original = model.ui()->theme();
            QVERIFY(!original.isEmpty());

            QSignalSpy themeSpy(model.ui(), &ssa::presentation::UiSettingsViewModel::themeChanged);
            model.ui()->setTheme("dark");
            QCOMPARE(model.ui()->theme(), QString("dark"));
            QVERIFY(themeSpy.size() >= 1);

            // Simulate ThemeDialog::reject() restoring the original theme
            themeSpy.clear();
            model.ui()->setTheme(original);
            QCOMPARE(model.ui()->theme(), original);
            QVERIFY(themeSpy.size() >= 1);
        }

        void details_visibility_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDetailsVisible(false);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->detailsVisible(), false);
            QCOMPARE(preferences->snapshot().detailsVisible, false);
        }

        void details_panel_width_preference_is_saved_and_clamped() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDetailsPanelWidth(620);
            model.ui()->setDetailsPanelWidth(900);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->detailsPanelWidth(), 900);
            QCOMPARE(preferences->snapshot().detailsPanelWidth, 900);
        }

        void density_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDensity("comfortable");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->density(), QString("comfortable"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().density),
                     QString("comfortable"));
        }

        void saved_filter_apply_matches_name_case_insensitively() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->search()->setText("saved query");
            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                              Q_ARG(QString, QString("Mixed Case"))));
            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 1);

            model.browse()->search()->setText("changed query");
            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "applySavedFilter",
                                              Q_ARG(QString, QString("mixed case"))));

            QCOMPARE(model.browse()->search()->text(), QString("saved query"));
        }

        void saved_filter_remove_matches_name_case_insensitively() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->search()->setText("active query");
            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                              Q_ARG(QString, QString("Mixed Case"))));
            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 1);

            QVERIFY(QMetaObject::invokeMethod(model.preferenceFlow(), "removeSavedFilter",
                                              Q_ARG(QString, QString("MIXED CASE"))));

            QCOMPARE(model.preferenceFlow()->property("savedFilters").toList().size(), 0);
        }

        void current_week_exposes_iso_label_for_header() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QVERIFY(
                model.actions()->currentWeek()->value().contains(QRegularExpression("^\\d{6}$")));
            QVERIFY(model.actions()->currentWeek()->dateTimeLabel().contains(
                QRegularExpression("^\\d{2}/\\d{2}/\\d{4} \\d{2}:\\d{2}$")));
            QVERIFY(!model.actions()->currentWeek()->dateTimeLabel().contains(
                model.actions()->currentWeek()->value()));
        }

        void rescan_incremental_uses_workflow_port_and_updates_status() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->rescanIncremental();

            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->requests().back().mode, ssa::ports::RescanMode::Incremental);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Reescaneamento concluido"), 1000);
        }

        void analytics_is_exposed_and_invalidated_after_successful_import() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            auto analyticsPort = std::make_shared<BlockingActivityAnalyticsPort>();
            auto analyticsService =
                std::make_shared<ssa::application::ActivityAnalyticsService>(analyticsPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows,
                                                   nullptr, nullptr, nullptr, nullptr,
                                                   analyticsService);

            QVERIFY(model.analytics() != nullptr);
            QSignalSpy invalidatedSpy(model.analytics(),
                                      &ssa::presentation::ActivityAnalyticsViewModel::invalidated);

            model.actions()->workflows()->rescanIncremental();

            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(invalidatedSpy.size(), 1, 1000);
        }

        void global_cancel_stops_active_analytics_query() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto analyticsPort = std::make_shared<BlockingActivityAnalyticsPort>(true);
            auto analyticsService =
                std::make_shared<ssa::application::ActivityAnalyticsService>(analyticsPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, nullptr,
                                                   nullptr, nullptr, nullptr, nullptr,
                                                   analyticsService);
            bool sawCancelableState = false;
            bool sawTerminalState = false;
            connect(&model, &ssa::presentation::MainViewModel::activityStateChanged, &model, [&] {
                const bool cancelable = model.canCancelActivity();
                sawCancelableState = sawCancelableState || cancelable;
                sawTerminalState = sawTerminalState || (sawCancelableState && !cancelable &&
                                                        !model.cancelingActivity());
            });
            const ssa::domain::AnalyticsRequest request{
                .metric = ssa::domain::AnalyticsMetric::Executed,
                .period = {.first = {2026, 1}, .last = {2026, 2}},
                .grain = ssa::domain::TimeGrain::WholePeriod,
                .breakdown = ssa::domain::Breakdown::Division,
                .personRole = ssa::domain::PersonRole::Executor,
                .divisions = {"DIV"},
            };

            model.analytics()->loadCustomSeries(request);
            QTRY_VERIFY_WITH_TIMEOUT(analyticsPort->started(), 1000);
            QVERIFY(model.canCancelActivity());
            QTRY_VERIFY_WITH_TIMEOUT(sawCancelableState, 1000);

            model.requestCancelAll();

            QTRY_VERIFY_WITH_TIMEOUT(analyticsPort->stopObserved(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.analytics()->loading(), 1000);
            QVERIFY(!model.canCancelActivity());
            QTRY_VERIFY_WITH_TIMEOUT(sawTerminalState, 1000);
        }

        void import_external_files_uses_workflow_port_and_updates_status() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl::fromLocalFile("/tmp/entrada.xlsx"));
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->importRequests().back().files.size(), std::size_t{1});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Importacao concluida"), 1000);
        }

        void import_success_with_warning_reloads_and_preserves_warning_detail() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>(ssa::ports::WorkflowResult{
                ssa::ports::WorkflowStatus::Succeeded, "consolidation canceled", true});
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            QSignalSpy pageSpy(model.browse(), &ssa::presentation::BrowseViewModel::pageChanged);
            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl::fromLocalFile("/tmp/entrada.xlsx"));

            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(repository->countCalls(), std::size_t{1}, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(pageSpy.size() >= 1, 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Importacao concluida com avisos: consolidacao "
                                              "cancelada"),
                                      1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->error(), QString{}, 1000);
        }

        void import_external_files_rejects_non_local_url() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl("https://example.com/entrada.xlsx"));
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{0}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao importar arquivos"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("Falha ao importar arquivos: apenas arquivos locais sao suportados"), 1000);
        }

        void import_external_files_rejects_empty_selection() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{0}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao importar arquivos"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("Falha ao importar arquivos: nenhum arquivo selecionado"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void orphan_derivation_cleanup_updates_status_after_success() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->cleanOrphanDerivations();

            QTRY_COMPARE_WITH_TIMEOUT(derivadasPort->syncCalls(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Limpeza de referencias orfas concluida"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
        }

        void derivadas_import_dispatches_local_files_and_updates_status() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            QCOMPARE(model.actions()->workflows()->legacyDerivadasConverterAvailable(), true);
            ssa::ports::UserPreferencesSnapshot preferences;
            preferences.importExecution.rowsPerChunk = 321;
            preferences.importExecution.sqliteBusyWaitMs = 125;
            model.actions()->workflows()->applyPreferences(preferences);
            const auto first = QDir::temp().filePath(QStringLiteral("derivadas.csv"));
            const auto second = QDir::temp().filePath(QStringLiteral("derivadas.xlsx"));
            QVariantList selectedFiles{QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)};

            model.actions()->workflows()->importDerivations(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(derivadasPort->importRequests().size(), std::size_t{1}, 1000);
            const auto requests = derivadasPort->importRequests();
            QCOMPARE(requests.front().files.size(), std::size_t{2});
            QCOMPARE(requests.front().files[0], ssa::qt::toFileSystemPath(first));
            QCOMPARE(requests.front().files[1], ssa::qt::toFileSystemPath(second));
            QCOMPARE(requests.front().execution.rowsPerChunk, std::size_t{321});
            QCOMPARE(requests.front().execution.sqliteBusyWait, std::chrono::milliseconds{125});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Importacao de derivadas concluida"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
        }

        void compact_database_runs_maintenance_workflow() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto maintenancePort = std::make_shared<CapturingMaintenancePort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, maintenancePort, nullptr);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->compactDatabase();

            QTRY_COMPARE_WITH_TIMEOUT(maintenancePort->vacuumAnalyzeCalls(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Banco compactado"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
        }

        void workflow_success_invalidates_total_rows_all_before_refresh() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            QSignalSpy pageSpy(model.browse(), &ssa::presentation::BrowseViewModel::pageChanged);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(repository->countCalls(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(pageSpy.size(), 1, 1000);

            model.actions()->workflows()->cleanOrphanDerivations();

            QTRY_COMPARE_WITH_TIMEOUT(repository->countCalls(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Limpeza de referencias orfas concluida"), 1000);
        }

        void orphan_derivation_cleanup_reports_workflow_error() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>(
                ssa::ports::WorkflowResult{ssa::ports::WorkflowStatus::Failed,
                                           "orphan derivation cleanup failed in integration path"});
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->cleanOrphanDerivations();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao limpar referencias orfas"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("orphan derivation cleanup failed in integration path"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void orphan_derivation_cleanup_reports_not_configured_adapter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, nullptr);

            model.actions()->workflows()->cleanOrphanDerivations();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao limpar referencias orfas"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("orphan derivation cleanup workflow is not configured"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void rescan_full_uses_full_mode() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->rescanFull();

            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->requests().back().mode, ssa::ports::RescanMode::Full);
        }

        void invalid_density_is_ignored() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDensity("wide");

            QCOMPARE(model.ui()->density(), QString("compact"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void column_settings_discard_restores_applied_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "discardColumnSettings");
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QCOMPARE(repository->requests().size(), std::size_t{0});
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 140,
                                      1000);
        }

        void command_view_model_uses_external_command_port() {
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openSsa("202500001");

            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            const auto executed = commands->commands().front();
            QCOMPARE(executed.kind, ssa::ports::ExternalCommandKind::OpenSsa);
            QCOMPARE(QString::fromStdString(executed.parameters.at("ssa_number")),
                     QString("202500001"));
            QTRY_COMPARE_WITH_TIMEOUT(model.lastMessage(), QString("ok"), 1000);
            QCOMPARE(model.lastStatus(), QString("succeeded"));
            QCOMPARE(model.lastSucceeded(), true);
        }

        void export_destruction_requests_stop_without_delivering_callback() {
            auto exportPort = std::make_shared<BlockingCancelableExportPort>();
            const auto releaseTerminal = qScopeGuard([&] { exportPort->releaseTerminal(); });
            auto workflows =
                std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
            int callbackCount = 0;
            QElapsedTimer destructionTimer;
            {
                ssa::presentation::ExportViewModel viewModel(
                    workflows, [] { return ssa::domain::SsaPageRequest{}; });
                connect(&viewModel, &ssa::presentation::ExportViewModel::lastResultChanged, this,
                        [&callbackCount] { ++callbackCount; });
                viewModel.exportFilteredList(QUrl::fromLocalFile("/tmp/ssa-export-cancel.csv"));
                QTRY_VERIFY_WITH_TIMEOUT(exportPort->started(), 1000);
                destructionTimer.start();
            }

            QVERIFY(destructionTimer.elapsed() < 50);
            QTRY_VERIFY_WITH_TIMEOUT(exportPort->stopObserved(), 1000);
            QVERIFY(!exportPort->finished());
            exportPort->releaseTerminal();
            QTRY_VERIFY_WITH_TIMEOUT(exportPort->finished(), 1000);
            QVERIFY(!exportPort->stopWaitTimedOut());
            QVERIFY(!exportPort->terminalTimedOut());
            QCoreApplication::processEvents();
            QCOMPARE(callbackCount, 0);
        }

        void export_cancel_returnsImmediatelyAndWaitsForTheRealTerminal() {
            auto exportPort = std::make_shared<DelayedTerminalExportPort>();
            auto workflows =
                std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
            ssa::presentation::ExportViewModel viewModel(
                workflows, [] { return ssa::domain::SsaPageRequest{}; });

            viewModel.exportFilteredList(QUrl::fromLocalFile("/tmp/ssa-export-cancel.csv"));
            QTRY_VERIFY_WITH_TIMEOUT(exportPort->started(), 1000);
            QVERIFY(viewModel.canCancel());

            QElapsedTimer timer;
            timer.start();
            viewModel.cancel();
            viewModel.cancel();

            QVERIFY(timer.elapsed() < 50);
            QVERIFY(viewModel.running());
            QVERIFY(viewModel.canceling());
            QVERIFY(!viewModel.canCancel());
            QTRY_VERIFY_WITH_TIMEOUT(exportPort->stopObserved(), 1000);

            exportPort->release();
            QTRY_VERIFY_WITH_TIMEOUT(!viewModel.running(), 1000);
            QVERIFY(!viewModel.canceling());
            QCOMPARE(viewModel.lastStatus(), QString("canceled"));
        }

        void export_cancel_is_not_reported_as_failure_in_global_status() {
            auto exportPort = std::make_shared<DelayedTerminalExportPort>();
            auto workflows =
                std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            model.actions()->exports()->exportFilteredList(
                QUrl::fromLocalFile("/tmp/ssa-export-cancel.csv"));
            QTRY_VERIFY_WITH_TIMEOUT(exportPort->started(), 1000);

            model.requestCancelAll();
            exportPort->release();

            QTRY_VERIFY_WITH_TIMEOUT(!model.actions()->exports()->running(), 1000);
            QCOMPARE(model.browse()->status()->message(), QString("Exportacao cancelada"));
            QCOMPARE(model.browse()->status()->error(), QString());
        }

        void application_shutdownKeepsEventsResponsiveUntilAllTerminals() {
            auto exportPort = std::make_shared<DelayedTerminalExportPort>();
            auto workflows =
                std::make_shared<ssa::application::SsaWorkflowService>(nullptr, exportPort);
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);
            model.actions()->exports()->exportFilteredList(
                QUrl::fromLocalFile("/tmp/ssa-shutdown-export.csv"));
            QTRY_VERIFY_WITH_TIMEOUT(exportPort->started(), 1000);

            bool eventDelivered = false;
            QTimer::singleShot(0, this, [&eventDelivered] { eventDelivered = true; });
            QElapsedTimer timer;
            timer.start();
            model.requestShutdown();

            QVERIFY(timer.elapsed() < 50);
            QVERIFY(model.shutdownInProgress());
            QVERIFY(!model.shutdownReady());
            QTRY_VERIFY_WITH_TIMEOUT(eventDelivered, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(exportPort->stopObserved(), 1000);
            QVERIFY(!model.shutdownReady());

            exportPort->release();
            QTRY_VERIFY_WITH_TIMEOUT(model.shutdownReady(), 1000);
        }

        void application_shutdown_waits_for_canceled_relation_terminal() {
            auto repository = std::make_shared<SlowDetailsRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            model.browse()->details()->setRecord(
                ssa::domain::SsaRecord{{{"numero_ssa", "202500001"}, {"situacao", "APV"}}});
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            model.requestShutdown();

            QVERIFY(!model.shutdownReady());
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(model.shutdownReady(), 1000);
        }

        void application_shutdown_tracks_detached_details_window_query() {
            auto repository = std::make_shared<SlowRelationNavigationRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* details = model.browse()->createDetailsWindowModel(QStringLiteral("old"), &model);
            QVERIFY(details != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);

            model.requestShutdown();

            QVERIFY(!model.shutdownReady());
            QTRY_VERIFY_WITH_TIMEOUT(repository->stopObserved(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(model.shutdownReady(), 1000);
        }

        void application_shutdown_waits_for_stale_page_worker_terminal() {
            auto repository = std::make_shared<SlowCancelableRepository>(false, true);
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            model.browse()->search()->setText(QStringLiteral("first"));
            model.browse()->apply();
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstStarted(), 1000);
            model.browse()->search()->setText(QStringLiteral("second"));
            model.browse()->apply();
            QTRY_VERIFY_WITH_TIMEOUT(repository->secondStarted(), 1000);
            QVERIFY(!repository->firstFinished());

            model.requestShutdown();
            QTest::qWait(100);

            const bool readyBeforeRelease = model.shutdownReady();
            repository->releaseFirst();
            QVERIFY(!readyBeforeRelease);
            QTRY_VERIFY_WITH_TIMEOUT(repository->firstFinished(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(model.shutdownReady(), 1000);
        }

        void command_view_model_executes_port_on_owner_thread() {
            auto commands = std::make_shared<ThreadCapturingCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openSamHome();

            QTRY_VERIFY_WITH_TIMEOUT(commands->executedThread() != nullptr, 1000);
            QCOMPARE(commands->executedThread(), model.thread());
        }

        void command_view_model_destruction_drops_queued_completion() {
            auto commands = std::make_shared<ThreadCapturingCommands>();
            int resultCallbackCount = 0;
            {
                ssa::presentation::CommandViewModel model(commands);
                connect(&model, &ssa::presentation::CommandViewModel::lastResultChanged, this,
                        [&resultCallbackCount] { ++resultCallbackCount; });
                model.openSamHome();
            }

            QCoreApplication::processEvents();
            QCOMPARE(commands->executedThread(), nullptr);
            QCOMPARE(resultCallbackCount, 0);
        }

        void command_view_model_rejects_wrong_thread_without_mutating_state() {
            auto commands = std::make_shared<ThreadCapturingCommands>();
            ssa::presentation::CommandViewModel model(commands);

            std::thread worker([&model] { model.openSamHome(); });
            worker.join();

            QCOMPARE(commands->executedThread(), nullptr);
            QCOMPARE(model.lastStatus(), QString("idle"));
            QCOMPARE(model.lastMessage(), QString());
        }

        void command_view_model_preserves_not_implemented_status() {
            auto commands = std::make_shared<FakeCommands>();
            commands->nextResult = {ssa::ports::ExternalCommandStatus::NotImplemented,
                                    "not implemented"};
            ssa::presentation::CommandViewModel model(commands);

            model.openSamHome();

            QTRY_COMPARE_WITH_TIMEOUT(model.lastStatus(), QString("not_implemented"), 1000);
            QCOMPARE(model.lastSucceeded(), false);
        }

        void command_view_model_exposes_configured_local_paths() {
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openInputFolder();

            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            QCOMPARE(commands->commands().front().kind,
                     ssa::ports::ExternalCommandKind::OpenInputFolder);
            QTRY_COMPARE_WITH_TIMEOUT(model.lastStatus(), QString("succeeded"), 1000);
        }

        void column_move_reorders_visible_keys() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            // Find the first two visible columns to swap.
            const auto keys = model.columns()->visibleKeys();
            QVERIFY(keys.size() >= 2);
            const auto keyA = QString::fromStdString(keys[0]);
            const auto keyB = QString::fromStdString(keys[1]);

            // Find the model rows for these two keys in a single pass.
            int rowA = -1;
            int rowB = -1;
            for (int i = 0; i < model.columns()->rowCount(); ++i) {
                const auto key = model.columns()
                                     ->data(model.columns()->index(i),
                                            ssa::presentation::ColumnSettingsModel::KeyRole)
                                     .toString();
                if (key == keyA) {
                    rowA = i;
                }
                if (key == keyB) {
                    rowB = i;
                }
            }
            QVERIFY(rowA >= 0);
            QVERIFY(rowB >= 0);

            const auto allKeys = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(allKeys[0]), keyA);
            QCOMPARE(QString::fromStdString(allKeys[1]), keyB);

            const bool moved = model.columns()->moveColumn(rowA, rowB);
            QCOMPARE(moved, true);

            const auto keysAfter = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(keysAfter[0]), keyB);
            QCOMPARE(QString::fromStdString(keysAfter[1]), keyA);
        }

        void column_move_persists_order_through_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"situacao", "numero_ssa"};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            const auto initialKeys = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(initialKeys[0]), QString("situacao"));
            QCOMPARE(QString::fromStdString(initialKeys[1]), QString("numero_ssa"));

            // Move column 0 (situacao) to position 1, swapping the visible order.
            int rowA = -1;
            int rowB = -1;
            for (int i = 0; i < model.columns()->rowCount(); ++i) {
                const auto key = model.columns()
                                     ->data(model.columns()->index(i),
                                            ssa::presentation::ColumnSettingsModel::KeyRole)
                                     .toString();
                if (key == QString("situacao")) {
                    rowA = i;
                }
                if (key == QString("numero_ssa")) {
                    rowB = i;
                }
            }
            QVERIFY(rowA >= 0);
            QVERIFY(rowB >= 0);

            const bool moved = model.columns()->moveColumn(rowA, rowB);
            QCOMPARE(moved, true);
            qobject_cast<ssa::presentation::MainColumnFlowCoordinator*>(model.columnFlow())
                ->applyColumnSettings();

            // Verify the in-memory order flipped.
            const auto reordered = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(reordered[0]), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(reordered[1]), QString("situacao"));

            // Verify the new order actually persisted into the preferences store.
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.front(),
                                      std::string("numero_ssa"), 1000);
            QCOMPARE(preferences->snapshot().visibleColumns.at(1), std::string("situacao"));
        }

        void column_header_move_reorders_and_persists_visible_order() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"situacao", "numero_ssa"};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            auto* flow =
                qobject_cast<ssa::presentation::MainColumnFlowCoordinator*>(model.columnFlow());
            QVERIFY(flow != nullptr);
            QVERIFY(flow->moveVisibleColumnAndApply(0, 1));
            const std::vector<std::string> expectedOrder{"numero_ssa", "situacao"};
            QCOMPARE(model.browse()->visibleColumns(), expectedOrder);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.front(),
                                      std::string("numero_ssa"), 1000);
            QCOMPARE(preferences->snapshot().visibleColumns.at(1), std::string("situacao"));
        }

        void column_reset_restores_catalog_order() {
            ssa::presentation::ColumnSettingsModel model;
            const auto initialKeys = model.visibleKeys();
            QVERIFY(initialKeys.size() >= 2);

            int firstRow = -1;
            int secondRow = -1;
            for (int row = 0; row < model.rowCount(); ++row) {
                const auto key =
                    model.data(model.index(row), ssa::presentation::ColumnSettingsModel::KeyRole)
                        .toString();
                if (key == QString::fromStdString(initialKeys[0])) {
                    firstRow = row;
                }
                if (key == QString::fromStdString(initialKeys[1])) {
                    secondRow = row;
                }
            }
            QVERIFY(firstRow >= 0);
            QVERIFY(secondRow >= 0);
            QVERIFY(model.moveColumn(firstRow, secondRow));
            QCOMPARE(QString::fromStdString(model.visibleKeys()[0]),
                     QString::fromStdString(initialKeys[1]));

            model.resetDefaults();
            const auto restoredKeys = model.visibleKeys();
            QCOMPARE(QString::fromStdString(restoredKeys[0]),
                     QString::fromStdString(initialKeys[0]));
            QCOMPARE(QString::fromStdString(restoredKeys[1]),
                     QString::fromStdString(initialKeys[1]));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationSmokeTest)

#include "PresentationSmokeTest.moc"
