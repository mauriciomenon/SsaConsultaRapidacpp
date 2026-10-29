#include "presentation/WorkflowCommandRunner.h"
#include "application/SsaWorkflowService.h"
#include "presentation/WorkflowCommandViewModel.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>

namespace {

    class BlockingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request,
                            const std::stop_token stopToken) override {
            return waitForCancellation(stopToken, request.progress);
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest& request,
                                          const std::stop_token stopToken) override {
            return waitForCancellation(stopToken, request.progress);
        }

        [[nodiscard]] bool waitUntilStarted(const std::chrono::milliseconds timeout) {
            std::unique_lock lock(mutex_);
            return changed_.wait_for(lock, timeout, [this] { return started_; });
        }

        [[nodiscard]] bool canceled() const {
            const std::scoped_lock lock(mutex_);
            return canceled_;
        }

        [[nodiscard]] bool finished() const {
            const std::scoped_lock lock(mutex_);
            return finished_;
        }

        [[nodiscard]] bool stopObserved() const {
            const std::scoped_lock lock(mutex_);
            return stopObserved_;
        }

        [[nodiscard]] bool stopWaitTimedOut() const {
            const std::scoped_lock lock(mutex_);
            return stopWaitTimedOut_;
        }

        [[nodiscard]] bool terminalTimedOut() const {
            const std::scoped_lock lock(mutex_);
            return terminalTimedOut_;
        }

        void releaseTerminal() {
            {
                const std::scoped_lock lock(mutex_);
                releaseTerminal_ = true;
            }
            changed_.notify_all();
        }

        [[nodiscard]] int calls() const {
            const std::scoped_lock lock(mutex_);
            return calls_;
        }

      private:
        ssa::ports::WorkflowResult
        waitForCancellation(const std::stop_token stopToken,
                            const ssa::ports::WorkflowProgressCallback& progress) {
            std::unique_lock lock(mutex_);
            ++calls_;
            started_ = true;
            changed_.notify_all();
            changed_.wait_for(lock, stopToken, std::chrono::seconds{2}, [] { return false; });
            stopObserved_ = stopToken.stop_requested();
            stopWaitTimedOut_ = !stopObserved_;
            canceled_ = stopObserved_;
            if (progress) {
                progress({ssa::ports::WorkflowProgressStage::ProcessingFile,
                          ssa::ports::WorkflowProgressLevel::Information,
                          1,
                          1,
                          50,
                          "shutdown.xlsx",
                          "late progress",
                          {}});
            }
            changed_.notify_all();
            const bool released = changed_.wait_for(lock, std::chrono::seconds{2},
                                                    [this] { return releaseTerminal_; });
            terminalTimedOut_ = !released;
            finished_ = true;
            lock.unlock();
            return {ssa::ports::WorkflowStatus::Canceled, "workflow canceled"};
        }

        mutable std::mutex mutex_;
        std::condition_variable_any changed_;
        bool started_ = false;
        bool canceled_ = false;
        bool finished_ = false;
        bool stopObserved_ = false;
        bool stopWaitTimedOut_ = false;
        bool terminalTimedOut_ = false;
        bool releaseTerminal_ = false;
        int calls_ = 0;
    };

    class BlockingSamPort final : public ssa::ports::ISamRefreshPort {
      public:
        ssa::ports::SamFetchResult fetch(const ssa::ports::SamRefreshRequest& request,
                                         const std::stop_token stopToken) override {
            std::unique_lock lock(mutex_);
            request_ = request;
            ++calls_;
            started_ = true;
            changed_.notify_all();
            changed_.wait(lock, stopToken, [] { return false; });
            canceled_ = true;
            return {ssa::ports::WorkflowStatus::Canceled, "SAM refresh canceled", {}};
        }

        bool discardArtifacts() override {
            return true;
        }

        [[nodiscard]] bool waitUntilStarted(const std::chrono::milliseconds timeout) {
            std::unique_lock lock(mutex_);
            return changed_.wait_for(lock, timeout, [this] { return started_; });
        }

        [[nodiscard]] bool canceled() const {
            const std::scoped_lock lock(mutex_);
            return canceled_;
        }

        [[nodiscard]] int calls() const {
            const std::scoped_lock lock(mutex_);
            return calls_;
        }

        [[nodiscard]] std::optional<ssa::ports::SamRefreshRequest> request() const {
            const std::scoped_lock lock(mutex_);
            return request_;
        }

      private:
        mutable std::mutex mutex_;
        std::condition_variable_any changed_;
        bool started_ = false;
        bool canceled_ = false;
        int calls_ = 0;
        std::optional<ssa::ports::SamRefreshRequest> request_;
    };

    class ImmediateImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request,
                            const std::stop_token stopToken) override {
            {
                const std::scoped_lock lock(mutex_);
                lastImportRequest_ = request;
                lastImportRequest_->progress = {};
                importProgressAttached_ = static_cast<bool>(request.progress);
            }
            reportProgress(request.progress, "import.xlsx");
            return finish(stopToken);
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest& request,
                                          const std::stop_token stopToken) override {
            {
                const std::scoped_lock lock(mutex_);
                lastRescanRequest_ = request;
                lastRescanRequest_->progress = {};
                rescanProgressAttached_ = static_cast<bool>(request.progress);
            }
            reportProgress(request.progress, "rescan.xlsx");
            return finish(stopToken);
        }

        [[nodiscard]] int calls() const {
            return calls_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool stopObserved() const {
            return stopObserved_.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::optional<ssa::ports::ImportExternalFilesRequest>
        lastImportRequest() const {
            const std::scoped_lock lock(mutex_);
            return lastImportRequest_;
        }

        [[nodiscard]] std::optional<ssa::ports::RescanRequest> lastRescanRequest() const {
            const std::scoped_lock lock(mutex_);
            return lastRescanRequest_;
        }

        [[nodiscard]] bool importProgressAttached() const {
            const std::scoped_lock lock(mutex_);
            return importProgressAttached_;
        }

        [[nodiscard]] bool rescanProgressAttached() const {
            const std::scoped_lock lock(mutex_);
            return rescanProgressAttached_;
        }

      private:
        static void reportProgress(const ssa::ports::WorkflowProgressCallback& progress,
                                   const std::string& fileName) {
            if (!progress) {
                return;
            }
            progress({ssa::ports::WorkflowProgressStage::ProcessingFile,
                      ssa::ports::WorkflowProgressLevel::Information, 1, 1, 50, fileName,
                      "processing", "information detail"});
            progress({ssa::ports::WorkflowProgressStage::Consolidating,
                      ssa::ports::WorkflowProgressLevel::Warning, 1, 1, 90, fileName,
                      "source warning", "warning detail"});
            progress({ssa::ports::WorkflowProgressStage::Completed,
                      ssa::ports::WorkflowProgressLevel::Warning, 1, 1, 100, fileName, "succeeded",
                      "terminal diagnostic"});
        }

        ssa::ports::WorkflowResult finish(const std::stop_token stopToken) {
            calls_.fetch_add(1, std::memory_order_acq_rel);
            stopObserved_.store(stopToken.stop_requested(), std::memory_order_release);
            return {stopToken.stop_requested() ? ssa::ports::WorkflowStatus::Canceled
                                               : ssa::ports::WorkflowStatus::Succeeded,
                    stopToken.stop_requested() ? "canceled" : "succeeded",
                    !stopToken.stop_requested(),
                    stopToken.stop_requested() ? std::string{} : "terminal diagnostic"};
        }

        std::atomic_int calls_{0};
        std::atomic_bool stopObserved_{false};
        mutable std::mutex mutex_;
        std::optional<ssa::ports::ImportExternalFilesRequest> lastImportRequest_;
        std::optional<ssa::ports::RescanRequest> lastRescanRequest_;
        bool importProgressAttached_ = false;
        bool rescanProgressAttached_ = false;
    };

    class ProgressDerivadasPort final : public ssa::ports::IDerivadasPort {
      public:
        [[nodiscard]] bool legacySpreadsheetConverterAvailable() const override {
            return true;
        }

        ssa::ports::WorkflowResult
        importDerivations(const ssa::ports::ImportDerivationsRequest& request,
                          std::stop_token = {}) override {
            progressAttached_ = static_cast<bool>(request.progress);
            if (request.progress) {
                request.progress({ssa::ports::WorkflowProgressStage::ProcessingFile,
                                  ssa::ports::WorkflowProgressLevel::Information, 1, 1, 50,
                                  "derivadas.xlsx", "Processando derivadas",
                                  "1 relacao identificada"});
            }
            return {ssa::ports::WorkflowStatus::Succeeded, "derivadas import completed"};
        }

        ssa::ports::WorkflowResult cleanOrphanDerivations(std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::Succeeded, "orphan cleanup completed"};
        }

        [[nodiscard]] bool progressAttached() const noexcept {
            return progressAttached_;
        }

      private:
        bool progressAttached_ = false;
    };

    class SummaryImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request,
                            std::stop_token = {}) override {
            ssa::ports::ImportSummary summary;
            summary.discovered = request.files.size();
            summary.accepted = request.files.size();
            summary.inserts = 3;
            for (const auto& file : request.files) {
                ssa::ports::ImportFileResult applied;
                applied.source = file.filename().string();
                applied.status = ssa::ports::ImportFileStatus::Applied;
                applied.inserts = 3;
                applied.sourceModified = "2026-08-06 09:06:12";
                applied.sizeBytes = 2048;
                applied.duration = std::chrono::milliseconds{1500};
                summary.files.push_back(std::move(applied));
            }
            return {ssa::ports::WorkflowStatus::Succeeded,
                    "regular import completed",
                    false,
                    {},
                    std::move(summary)};
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest&,
                                          std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::Succeeded, "rescan completed"};
        }
    };

    class FailingDerivadasPort final : public ssa::ports::IDerivadasPort {
      public:
        [[nodiscard]] bool legacySpreadsheetConverterAvailable() const override {
            return false;
        }

        ssa::ports::WorkflowResult importDerivations(const ssa::ports::ImportDerivationsRequest&,
                                                     std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::Failed, "derivadas import failed", false,
                    "sqlite step failed: disk I/O error"};
        }

        ssa::ports::WorkflowResult cleanOrphanDerivations(std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::Succeeded, "orphan cleanup completed"};
        }
    };

    class WorkflowCommandRunnerTest final : public QObject {
        Q_OBJECT

      private slots:
        void view_model_reports_derivadas_failure_in_mixed_import_summary() {
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<SummaryImportPort>(), nullptr, nullptr,
                std::make_shared<FailingDerivadasPort>());
            ssa::presentation::WorkflowCommandViewModel model(workflows);
            QSignalSpy finished(
                &model, &ssa::presentation::WorkflowCommandViewModel::progressSessionFinished);
            QSignalSpy errors(&model,
                              &ssa::presentation::WorkflowCommandViewModel::progressErrorLine);
            QSignalSpy outputs(&model,
                               &ssa::presentation::WorkflowCommandViewModel::progressOutputLine);

            model.importExternalFiles(
                {QUrl::fromLocalFile("/tmp/Consulta SSA - 24-07-2026_0829AM.xlsx"),
                 QUrl::fromLocalFile("/tmp/SSAs Derivadas e Relacionadas_24-07-2026_0829AM.xlsx")});
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 1000);

            QVERIFY(model.lastSucceeded());
            QVERIFY(model.lastWarning());
            QVERIFY2(!model.lastMessage().contains(QStringLiteral("nenhuma falha")),
                     qPrintable(model.lastMessage()));
            QCOMPARE(model.lastMessage(),
                     QStringLiteral("Importacao parcial concluida: 2 arquivos examinados; "
                                    "1 aplicado; 1 falhou; 3 SSAs inseridas"));
            QCOMPARE(finished.at(0).at(2).toString(),
                     QStringLiteral("Importacao parcial concluida"));
            QStringList errorLines;
            for (const auto& call : errors) {
                errorLines.push_back(call.at(0).toString());
            }
            QVERIFY2(errorLines.contains(QStringLiteral("[FALHOU] SSAs Derivadas e "
                                                        "Relacionadas_24-07-2026_0829AM.xlsx - "
                                                        "falha operacional")),
                     qPrintable(errorLines.join(QStringLiteral(" | "))));

            QStringList outputLines;
            for (const auto& call : outputs) {
                outputLines.push_back(call.at(0).toString());
            }
            const auto appliedLine = std::ranges::find_if(outputLines, [](const QString& line) {
                return line.startsWith(
                    QStringLiteral("[OK] Consulta SSA - 24-07-2026_0829AM.xlsx: 3 SSAs novas | "
                                   "modificado 2026-08-06 09:06:12 | "));
            });
            QVERIFY2(appliedLine != outputLines.end(),
                     qPrintable(outputLines.join(QStringLiteral(" || "))));
            QVERIFY2(appliedLine->endsWith(QStringLiteral(" | 1,5 s")), qPrintable(*appliedLine));
            QVERIFY2(std::ranges::any_of(outputLines,
                                         [](const QString& line) {
                                             return line.startsWith(QStringLiteral(
                                                        "Resumo: 2 arquivos examinados; "
                                                        "1 aplicado; ")) &&
                                                    line.contains(QStringLiteral("1 falhou")) &&
                                                    line.contains(QStringLiteral("; duracao "));
                                         }),
                     qPrintable(outputLines.join(QStringLiteral(" || "))));
        }

        void destructor_requests_stop_without_callback() {
            auto importPort = std::make_shared<BlockingImportPort>();
            const auto releaseTerminal = qScopeGuard([&] { importPort->releaseTerminal(); });
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            int callbacks = 0;
            int progressCallbacks = 0;
            QElapsedTimer destructionTimer;

            {
                ssa::presentation::WorkflowCommandRunner runner(workflows);
                connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                        [&callbacks] { ++callbacks; });
                connect(&runner, &ssa::presentation::WorkflowCommandRunner::progressReported, this,
                        [&progressCallbacks] { ++progressCallbacks; });
                runner.rescan(ssa::ports::RescanMode::Incremental);
                QVERIFY(importPort->waitUntilStarted(std::chrono::seconds{1}));
                destructionTimer.start();
            }

            QVERIFY(destructionTimer.elapsed() < 50);
            QTRY_VERIFY_WITH_TIMEOUT(importPort->stopObserved(), 1000);
            QVERIFY(importPort->canceled());
            QVERIFY(!importPort->finished());
            importPort->releaseTerminal();
            QTRY_VERIFY_WITH_TIMEOUT(importPort->finished(), 1000);
            QVERIFY(!importPort->stopWaitTimedOut());
            QVERIFY(!importPort->terminalTimedOut());
            QCoreApplication::processEvents();
            QCOMPARE(callbacks, 0);
            QCOMPARE(progressCallbacks, 0);
        }

        void shutdown_rejects_new_work() {
            auto importPort = std::make_shared<BlockingImportPort>();
            const auto releaseTerminal = qScopeGuard([&] { importPort->releaseTerminal(); });
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);
            int progressCount = 0;
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::progressReported, this,
                    [&progressCount] { ++progressCount; });

            runner.rescan(ssa::ports::RescanMode::Incremental);
            QVERIFY(importPort->waitUntilStarted(std::chrono::seconds{1}));
            runner.shutdown();
            runner.rescan(ssa::ports::RescanMode::Incremental);

            QCOMPARE(importPort->calls(), 1);
            QCOMPARE(runner.state(), ssa::presentation::WorkflowCommandRunner::State::Canceling);
            QVERIFY(runner.running());
            QTRY_VERIFY_WITH_TIMEOUT(importPort->stopObserved(), 1000);
            QVERIFY(importPort->canceled());
            QCoreApplication::processEvents();
            QCOMPARE(progressCount, 0);
            QVERIFY(!importPort->finished());
            importPort->releaseTerminal();
            QTRY_VERIFY_WITH_TIMEOUT(importPort->finished(), 1000);
            QVERIFY(!importPort->stopWaitTimedOut());
            QVERIFY(!importPort->terminalTimedOut());
            QTRY_VERIFY_WITH_TIMEOUT(!runner.running(), 1000);
            QCOMPARE(progressCount, 0);
        }

        void progress_is_main_thread_only_and_other_operations_remain_terminal() {
            auto importPort = std::make_shared<ImmediateImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);
            int progressCount = 0;
            int finishedCount = 0;
            QThread* progressThread = nullptr;
            bool allProgressOnRunnerThread = true;
            std::vector<ssa::ports::WorkflowStatus> statuses;
            std::vector<ssa::ports::WorkflowProgressStage> progressStages;
            std::vector<QString> eventLog;
            const std::vector expectedStages{ssa::ports::WorkflowProgressStage::ProcessingFile,
                                             ssa::ports::WorkflowProgressStage::Consolidating,
                                             ssa::ports::WorkflowProgressStage::Completed};
            const std::vector expectedEvents{QStringLiteral("progress"), QStringLiteral("progress"),
                                             QStringLiteral("progress"),
                                             QStringLiteral("finished")};
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::progressReported, this,
                    [&](const ssa::ports::WorkflowProgress& progress) {
                        ++progressCount;
                        progressThread = QThread::currentThread();
                        allProgressOnRunnerThread =
                            allProgressOnRunnerThread && progressThread == runner.thread();
                        progressStages.push_back(progress.stage);
                        eventLog.push_back(QStringLiteral("progress"));
                    });
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                    [&](const ssa::ports::WorkflowResult& result) {
                        ++finishedCount;
                        statuses.push_back(result.status);
                        eventLog.push_back(QStringLiteral("finished"));
                    });

            runner.importExternalFiles({QStringLiteral("/tmp/import.xlsx")});
            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 1, 1000);
            QCOMPARE(progressCount, 3);
            QVERIFY(importPort->importProgressAttached());
            QCOMPARE(progressThread, runner.thread());
            QVERIFY(allProgressOnRunnerThread);
            QCOMPARE(progressStages, expectedStages);
            QCOMPARE(eventLog, expectedEvents);

            progressStages.clear();
            eventLog.clear();
            runner.rescan(ssa::ports::RescanMode::Incremental);
            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 2, 1000);
            QCOMPARE(progressCount, 6);
            QVERIFY(importPort->rescanProgressAttached());
            QCOMPARE(progressStages, expectedStages);
            QCOMPARE(eventLog, expectedEvents);

            runner.importDerivations({QStringLiteral("/tmp/derivadas.xlsx")});
            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 3, 1000);
            runner.refreshSam({});
            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 4, 1000);
            runner.cleanOrphanDerivations();
            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 5, 1000);
            runner.compactDatabase();
            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 6, 1000);

            QCOMPARE(progressCount, 6);
            QCOMPARE(statuses[0], ssa::ports::WorkflowStatus::Succeeded);
            QCOMPARE(statuses[1], ssa::ports::WorkflowStatus::Succeeded);
            for (std::size_t index = 2; index < statuses.size(); ++index) {
                QCOMPARE(statuses[index], ssa::ports::WorkflowStatus::NotImplemented);
            }
        }

        void derivadas_import_forwards_progress_to_the_main_thread() {
            auto derivadasPort = std::make_shared<ProgressDerivadasPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                nullptr, nullptr, nullptr, derivadasPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);
            int progressCount = 0;
            QThread* progressThread = nullptr;
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::progressReported, this,
                    [&](const ssa::ports::WorkflowProgress&) {
                        ++progressCount;
                        progressThread = QThread::currentThread();
                    });

            runner.importDerivations({QStringLiteral("/tmp/derivadas.xlsx")});

            QTRY_COMPARE_WITH_TIMEOUT(progressCount, 1, 1000);
            QVERIFY(derivadasPort->progressAttached());
            QCOMPARE(progressThread, runner.thread());
        }

        void view_model_publishes_one_progress_terminal_for_import_and_rescan() {
            auto importPort = std::make_shared<ImmediateImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandViewModel model(workflows);
            QSignalSpy started(
                &model, &ssa::presentation::WorkflowCommandViewModel::progressSessionStarted);
            QSignalSpy finished(
                &model, &ssa::presentation::WorkflowCommandViewModel::progressSessionFinished);
            QSignalSpy changed(&model,
                               &ssa::presentation::WorkflowCommandViewModel::progressChanged);
            QSignalSpy output(&model,
                              &ssa::presentation::WorkflowCommandViewModel::progressOutputLine);
            QSignalSpy errors(&model,
                              &ssa::presentation::WorkflowCommandViewModel::progressErrorLine);

            model.importExternalFiles({QUrl::fromLocalFile("/tmp/import.xlsx")});
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
            QCOMPARE(started.count(), 1);
            QCOMPARE(changed.count(), 3);
            QCOMPARE(output.count(), 1);
            QCOMPARE(errors.count(), 2);
            QCOMPARE(finished.at(0).at(0).toBool(), true);
            QCOMPARE(finished.at(0).at(1).toBool(), false);
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 1000);

            model.rescanFull();
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 1000);
            QCOMPARE(started.count(), 2);
            QCOMPARE(changed.count(), 6);
            QCOMPARE(output.count(), 2);
            QCOMPARE(errors.count(), 4);
            QCOMPARE(finished.at(1).at(0).toBool(), true);
            QCOMPARE(finished.at(1).at(1).toBool(), false);
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 1000);
        }

        void view_model_uses_one_fallback_terminal_for_config_failure() {
            ssa::presentation::WorkflowCommandViewModel model(nullptr);
            QSignalSpy started(
                &model, &ssa::presentation::WorkflowCommandViewModel::progressSessionStarted);
            QSignalSpy finished(
                &model, &ssa::presentation::WorkflowCommandViewModel::progressSessionFinished);
            QSignalSpy changed(&model,
                               &ssa::presentation::WorkflowCommandViewModel::progressChanged);
            QSignalSpy errors(&model,
                              &ssa::presentation::WorkflowCommandViewModel::progressErrorLine);

            model.importExternalFiles({QUrl::fromLocalFile("/tmp/import.xlsx")});

            QCOMPARE(started.count(), 1);
            QCOMPARE(changed.count(), 1);
            QCOMPARE(errors.count(), 1);
            QCOMPARE(finished.count(), 1);
            QCOMPARE(finished.at(0).at(0).toBool(), false);
            QCOMPARE(finished.at(0).at(1).toBool(), false);
        }

        void view_model_publishes_one_canceled_terminal() {
            auto importPort = std::make_shared<BlockingImportPort>();
            const auto releaseTerminal = qScopeGuard([&] { importPort->releaseTerminal(); });
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandViewModel model(workflows);
            QSignalSpy finished(
                &model, &ssa::presentation::WorkflowCommandViewModel::progressSessionFinished);
            QSignalSpy output(&model,
                              &ssa::presentation::WorkflowCommandViewModel::progressOutputLine);
            QSignalSpy errors(&model,
                              &ssa::presentation::WorkflowCommandViewModel::progressErrorLine);

            model.rescanIncremental();
            QVERIFY(importPort->waitUntilStarted(std::chrono::seconds{1}));
            model.cancel();
            QTRY_VERIFY_WITH_TIMEOUT(importPort->stopObserved(), 1000);
            importPort->releaseTerminal();
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);

            QCOMPARE(output.count(), 1);
            QCOMPARE(errors.count(), 1);
            QCOMPARE(finished.at(0).at(0).toBool(), false);
            QCOMPARE(finished.at(0).at(1).toBool(), true);
        }

        void sam_refresh_is_single_flight_and_canceled_on_shutdown() {
            auto samPort = std::make_shared<BlockingSamPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                nullptr, nullptr, nullptr, nullptr, samPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);
            ssa::ports::SamRefreshRequest request;
            request.enabled = true;

            runner.refreshSam(request);
            QVERIFY(samPort->waitUntilStarted(std::chrono::seconds{1}));
            runner.refreshSam(request);
            runner.shutdown();

            QCOMPARE(samPort->calls(), 1);
            QTRY_VERIFY_WITH_TIMEOUT(samPort->canceled(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!runner.running(), 1000);
        }

        void cancel_is_non_blocking_terminal_and_single_flight() {
            auto importPort = std::make_shared<BlockingImportPort>();
            const auto releaseTerminal = qScopeGuard([&] { importPort->releaseTerminal(); });
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);
            int finishedCount = 0;
            ssa::ports::WorkflowResult terminalResult;
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                    [&](const ssa::ports::WorkflowResult& result) {
                        ++finishedCount;
                        terminalResult = result;
                    });

            runner.rescan(ssa::ports::RescanMode::Incremental);
            QVERIFY(importPort->waitUntilStarted(std::chrono::seconds{1}));

            QElapsedTimer elapsed;
            elapsed.start();
            runner.cancel();

            QVERIFY(elapsed.elapsed() < 50);
            QCOMPARE(runner.state(), ssa::presentation::WorkflowCommandRunner::State::Canceling);
            QVERIFY(runner.running());
            QVERIFY(runner.canceling());
            QVERIFY(!runner.canCancel());
            QCOMPARE(finishedCount, 0);

            runner.cancel();
            runner.rescan(ssa::ports::RescanMode::Incremental);
            QCOMPARE(importPort->calls(), 1);
            QCOMPARE(finishedCount, 0);

            QTRY_VERIFY_WITH_TIMEOUT(importPort->stopObserved(), 1000);
            QVERIFY(importPort->canceled());
            QVERIFY(!importPort->finished());
            importPort->releaseTerminal();
            QTRY_VERIFY_WITH_TIMEOUT(importPort->finished(), 1000);
            QVERIFY(!importPort->stopWaitTimedOut());
            QVERIFY(!importPort->terminalTimedOut());
            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 1, 1000);
            QCOMPARE(terminalResult.status, ssa::ports::WorkflowStatus::Canceled);
            QCOMPARE(runner.state(), ssa::presentation::WorkflowCommandRunner::State::Idle);
            QVERIFY(!runner.running());
            QVERIFY(!runner.canceling());
            QVERIFY(!runner.canCancel());
        }

        void direct_running_slot_cancels_the_new_operation() {
            auto importPort = std::make_shared<ImmediateImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);
            ssa::ports::WorkflowResult terminalResult;
            int finishedCount = 0;
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::stateChanged, this,
                    [&](const ssa::presentation::WorkflowCommandRunner::State state) {
                        if (state == ssa::presentation::WorkflowCommandRunner::State::Running) {
                            runner.cancel();
                        }
                    });
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                    [&](const ssa::ports::WorkflowResult& result) {
                        terminalResult = result;
                        ++finishedCount;
                    });

            runner.rescan(ssa::ports::RescanMode::Incremental);

            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 1, 1000);
            QVERIFY(importPort->stopObserved());
            QCOMPARE(terminalResult.status, ssa::ports::WorkflowStatus::Canceled);
        }

        void terminal_slot_cannot_start_reentrant_workflow() {
            auto importPort = std::make_shared<ImmediateImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);
            int finishedCount = 0;
            connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                    [&](const ssa::ports::WorkflowResult&) {
                        ++finishedCount;
                        if (finishedCount == 1) {
                            runner.rescan(ssa::ports::RescanMode::Incremental);
                        }
                    });

            runner.rescan(ssa::ports::RescanMode::Incremental);

            QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 1, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(runner.state(),
                                      ssa::presentation::WorkflowCommandRunner::State::Idle, 1000);
            QCOMPARE(importPort->calls(), 1);
            QCOMPARE(runner.state(), ssa::presentation::WorkflowCommandRunner::State::Idle);
        }

        void sam_timer_does_not_fire_on_startup_and_skips_while_busy() {
            auto samPort = std::make_shared<BlockingSamPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                nullptr, nullptr, nullptr, nullptr, samPort);
            ssa::presentation::WorkflowCommandViewModel model(workflows);
            ssa::ports::UserPreferencesSnapshot preferences;
            preferences.samRefresh.enabled = true;
            preferences.samRefresh.autoRefreshEnabled = true;
            preferences.samRefresh.intervalMinutes = 1;
            preferences.samRefresh.scrapReportRoot = "/tmp/scrap_report";
            preferences.samRefresh.caFile = "/tmp/ca.pem";
            preferences.samRefresh.baseUrl = "https://apps.example.test/SAM/rest";
            preferences.samRefresh.executorSectors = "IEE3,MEL4";
            preferences.importExecution.rowsPerChunk = 321;
            preferences.importExecution.sqliteBusyWaitMs = 125;

            model.applyPreferences(preferences);

            QCOMPARE(samPort->calls(), 0);
            const auto timers = model.findChildren<QTimer*>();
            QCOMPARE(timers.size(), 1);
            QVERIFY(timers.front()->isActive());
            QCOMPARE(timers.front()->interval(), 60'000);

            QVERIFY(QMetaObject::invokeMethod(timers.front(), "timeout", Qt::DirectConnection));
            QVERIFY(samPort->waitUntilStarted(std::chrono::seconds{1}));
            QCOMPARE(samPort->request()->rowsPerChunk, std::size_t{321});
            QCOMPARE(samPort->request()->sqliteBusyWait, std::chrono::milliseconds{125});
            QVERIFY(QMetaObject::invokeMethod(timers.front(), "timeout", Qt::DirectConnection));
            QCOMPARE(samPort->calls(), 1);

            ssa::ports::UserPreferencesSnapshot written;
            model.writePreferences(written);
            QCOMPARE(written.samRefresh, preferences.samRefresh);
        }

        void import_execution_preferences_reach_import_and_rescan_requests() {
            auto importPort = std::make_shared<ImmediateImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandViewModel model(workflows);
            ssa::ports::UserPreferencesSnapshot preferences;
            preferences.importExecution.rowsPerChunk = 321;
            preferences.importExecution.sqliteBusyWaitMs = 125;
            model.applyPreferences(preferences);

            model.importExternalFiles({QUrl::fromLocalFile("/tmp/selected.xlsx")});
            QTRY_COMPARE_WITH_TIMEOUT(importPort->calls(), 1, 1000);
            const auto importRequest = importPort->lastImportRequest();
            QVERIFY(importRequest.has_value());
            QCOMPARE(importRequest->execution.rowsPerChunk, std::size_t{321});
            QCOMPARE(importRequest->execution.sqliteBusyWait, std::chrono::milliseconds{125});
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 1000);

            model.rescanIncremental();
            QTRY_COMPARE_WITH_TIMEOUT(importPort->calls(), 2, 1000);
            const auto rescanRequest = importPort->lastRescanRequest();
            QVERIFY(rescanRequest.has_value());
            QCOMPARE(rescanRequest->execution.rowsPerChunk, std::size_t{321});
            QCOMPARE(rescanRequest->execution.sqliteBusyWait, std::chrono::milliseconds{125});
        }
    };

} // namespace

QTEST_GUILESS_MAIN(WorkflowCommandRunnerTest)

#include "WorkflowCommandRunnerTest.moc"
