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
#include <vector>

namespace ssa::presentation {

    class WorkflowCommandRunner final : public QObject {
        Q_OBJECT

      public:
        explicit WorkflowCommandRunner(std::shared_ptr<application::SsaWorkflowService> workflows,
                                       QObject* parent = nullptr);
        ~WorkflowCommandRunner() override;

        [[nodiscard]] bool running() const;
        void importExternalFiles(const std::vector<QString>& files);
        void rescan(ports::RescanMode mode);
        void syncDerivadas();
        void compactDatabase();

      signals:
        void runningChanged(bool running);
        void finished(ssa::ports::WorkflowResult result);

      private:
        struct ResultState final {
            std::mutex mutex;
            std::optional<ports::WorkflowResult> result;
            std::exception_ptr error;
        };

        void start(std::function<ports::WorkflowResult()> operation);
        void finish();

        std::shared_ptr<application::SsaWorkflowService> workflows_;
        QFutureWatcher<void> watcher_;
        std::shared_ptr<ResultState> resultState_;
        bool running_{false};
    };

} // namespace ssa::presentation
