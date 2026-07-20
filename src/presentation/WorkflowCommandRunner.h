#pragma once

#include "application/SsaWorkflowService.h"

#include <QFutureWatcher>
#include <QObject>

#include <QString>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>

namespace ssa::presentation {

    class WorkflowCommandRunner final : public QObject {
        Q_OBJECT

      public:
        enum class State { Idle, Running, Canceling };
        Q_ENUM(State)

        explicit WorkflowCommandRunner(std::shared_ptr<application::SsaWorkflowService> workflows,
                                       QObject* parent = nullptr);
        ~WorkflowCommandRunner() override;

        [[nodiscard]] State state() const;
        [[nodiscard]] bool running() const;
        [[nodiscard]] bool canceling() const;
        [[nodiscard]] bool canCancel() const;
        [[nodiscard]] bool legacySpreadsheetConverterAvailable() const;
        void importExternalFiles(const std::vector<QString>& files,
                                 ports::ImportExecutionOptions execution = {});
        void importDerivations(const std::vector<QString>& files,
                               ports::ImportExecutionOptions execution = {});
        void rescan(ports::RescanMode mode, ports::ImportExecutionOptions execution = {});
        void refreshSam(ports::SamRefreshRequest request);
        void cleanOrphanDerivations();
        void compactDatabase();
        void cancel();
        void shutdown();

      signals:
        void stateChanged(ssa::presentation::WorkflowCommandRunner::State state);
        void runningChanged(bool running);
        void progressReported(ssa::ports::WorkflowProgress progress);
        void finished(ssa::ports::WorkflowResult result);

      private:
        struct ResultState final {
            std::mutex mutex;
            std::optional<ports::WorkflowResult> result = std::nullopt;
            std::exception_ptr error;
            std::vector<ports::WorkflowProgress> progress;
        };

        using Operation = std::function<ports::WorkflowResult(
            std::stop_token, const ports::WorkflowProgressCallback&)>;

        void start(Operation operation);
        void forwardProgress(int futureResultIndex);
        void finish();
        void setState(State state);

        std::shared_ptr<application::SsaWorkflowService> workflows_;
        QFutureWatcher<int> watcher_;
        std::shared_ptr<ResultState> resultState_;
        std::stop_source stopSource_;
        State state_{State::Idle};
        bool shuttingDown_ = false;
    };

} // namespace ssa::presentation
