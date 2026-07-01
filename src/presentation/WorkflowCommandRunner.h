#pragma once

#include "application/SsaWorkflowService.h"

#include <QFutureWatcher>
#include <QObject>

#include <QString>
#include <memory>
#include <vector>

namespace ssa::presentation {

    class WorkflowCommandRunner final : public QObject {
        Q_OBJECT

      public:
        explicit WorkflowCommandRunner(std::shared_ptr<application::SsaWorkflowService> workflows,
                                       QObject* parent = nullptr);

        [[nodiscard]] bool running() const;
        void importExternalFiles(const std::vector<QString>& files);
        void rescan(ports::RescanMode mode);
        void syncDerivadas();
        void compactDatabase();

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
