#pragma once

#include "application/SsaWorkflowService.h"

#include <QFutureWatcher>
#include <QObject>

#include <filesystem>
#include <memory>
#include <vector>

namespace ssa::presentation {

    class WorkflowCommandRunner final : public QObject {
        Q_OBJECT

      public:
        explicit WorkflowCommandRunner(std::shared_ptr<application::SsaWorkflowService> workflows,
                                       QObject* parent = nullptr);

        [[nodiscard]] bool running() const;
        void importExternalFiles(std::vector<std::filesystem::path> files);
        void rescan(ports::RescanMode mode);

      signals:
        void runningChanged(bool running);
        void finished(ssa::ports::WorkflowResult result);

      private:
        void finish();

        std::shared_ptr<application::SsaWorkflowService> workflows_;
        QFutureWatcher<ports::WorkflowResult> watcher_;
        bool running_{false};
    };

} // namespace ssa::presentation
