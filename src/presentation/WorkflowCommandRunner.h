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
        void importExternalFiles(const std::vector<QString>& files);
        void rescan(ports::RescanMode mode);
        void refreshSam(ports::SamRefreshRequest request);
        void syncDerivadas();
        void compactDatabase();
        void cancel();
        void shutdown();

      signals:
        void stateChanged(ssa::presentation::WorkflowCommandRunner::State state);
        void runningChanged(bool running);
        void finished(ssa::ports::WorkflowResult result);

      private:
        struct ResultState final {
            std::mutex mutex;
            std::optional<ports::WorkflowResult> result = std::nullopt;
            std::exception_ptr error;
        };

        void start(std::function<ports::WorkflowResult(std::stop_token)> operation);
        void finish();
        void setState(State state);

        std::shared_ptr<application::SsaWorkflowService> workflows_;
        QFutureWatcher<void> watcher_;
        std::shared_ptr<ResultState> resultState_;
        std::stop_source stopSource_;
        State state_{State::Idle};
        bool shuttingDown_ = false;
    };

} // namespace ssa::presentation
