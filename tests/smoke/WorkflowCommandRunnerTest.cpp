#include "presentation/WorkflowCommandRunner.h"
#include "application/SsaWorkflowService.h"

#include <QCoreApplication>
#include <QTest>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>

namespace {

    class BlockingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest&,
                            const std::stop_token stopToken) override {
            return waitForCancellation(stopToken);
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest&,
                                          const std::stop_token stopToken) override {
            return waitForCancellation(stopToken);
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

      private:
        ssa::ports::WorkflowResult waitForCancellation(const std::stop_token stopToken) {
            std::unique_lock lock(mutex_);
            ++calls_;
            started_ = true;
            changed_.notify_all();
            changed_.wait(lock, stopToken, [] { return false; });
            canceled_ = true;
            return {ssa::ports::WorkflowStatus::Rejected, "workflow canceled"};
        }

        mutable std::mutex mutex_;
        std::condition_variable_any changed_;
        bool started_ = false;
        bool canceled_ = false;
        int calls_ = 0;
    };

    class WorkflowCommandRunnerTest final : public QObject {
        Q_OBJECT

      private slots:
        void destructor_requests_stop_without_callback() {
            auto importPort = std::make_shared<BlockingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            int callbacks = 0;

            {
                ssa::presentation::WorkflowCommandRunner runner(workflows);
                connect(&runner, &ssa::presentation::WorkflowCommandRunner::finished, this,
                        [&callbacks] { ++callbacks; });
                runner.rescan(ssa::ports::RescanMode::Incremental);
                QVERIFY(importPort->waitUntilStarted(std::chrono::seconds{1}));
            }

            QVERIFY(importPort->canceled());
            QCoreApplication::processEvents();
            QCOMPARE(callbacks, 0);
        }

        void shutdown_rejects_new_work() {
            auto importPort = std::make_shared<BlockingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::WorkflowCommandRunner runner(workflows);

            runner.rescan(ssa::ports::RescanMode::Incremental);
            QVERIFY(importPort->waitUntilStarted(std::chrono::seconds{1}));
            runner.shutdown();
            runner.rescan(ssa::ports::RescanMode::Incremental);

            QCOMPARE(importPort->calls(), 1);
            QVERIFY(importPort->canceled());
            QVERIFY(!runner.running());
        }
    };

} // namespace

QTEST_GUILESS_MAIN(WorkflowCommandRunnerTest)

#include "WorkflowCommandRunnerTest.moc"
