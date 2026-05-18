#pragma once

#include "application/SsaWorkflowService.h"
#include "presentation/WorkflowCommandRunner.h"

#include <QObject>
#include <QString>

#include <memory>

namespace ssa::presentation {

    class WorkflowCommandViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY lastResultChanged)
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)

      public:
        explicit WorkflowCommandViewModel(
            std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent = nullptr);

        [[nodiscard]] QString lastMessage() const;
        [[nodiscard]] bool lastSucceeded() const;
        [[nodiscard]] bool running() const;

      signals:
        void lastResultChanged();
        void runningChanged();

      public slots:
        void rescanIncremental();
        void rescanFull();

      private:
        void startRescan(ports::RescanMode mode);
        void applyResult(const ports::WorkflowResult& result);
        void setRunning(bool running);
        void setResult(QString message, bool succeeded);

        WorkflowCommandRunner runner_;
        QString lastMessage_;
        bool lastSucceeded_{false};
        bool running_{false};
    };

} // namespace ssa::presentation
