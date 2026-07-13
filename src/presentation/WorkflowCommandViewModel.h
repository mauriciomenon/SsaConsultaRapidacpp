#pragma once

#include "application/SsaWorkflowService.h"
#include "presentation/WorkflowCommandRunner.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace ssa::presentation {

    class WorkflowCommandViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastWarning READ lastWarning NOTIFY lastResultChanged)
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)

      public:
        explicit WorkflowCommandViewModel(
            std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent = nullptr);

        [[nodiscard]] QString lastMessage() const;
        [[nodiscard]] bool lastSucceeded() const;
        [[nodiscard]] bool lastWarning() const;
        [[nodiscard]] bool running() const;
        [[nodiscard]] QString runningMessage() const;
        [[nodiscard]] QString successMessage() const;
        [[nodiscard]] QString failureMessage() const;

      signals:
        void lastResultChanged();
        void runningChanged();

      public slots:
        void importExternalFiles(const QVariantList& selectedFiles);
        void rescanIncremental();
        void rescanFull();
        void syncDerivadas();
        void compactDatabase();

      private:
        struct OperationMessages {
            QString running;
            QString success;
            QString failure;
        };

        void startRescan(ports::RescanMode mode);
        void applyResult(const ports::WorkflowResult& result);
        void setRunning(bool running);
        void setResult(QString message, bool succeeded, bool warning = false);
        [[nodiscard]] OperationMessages messagesForCurrentOperation() const;

        enum class Operation {
            Rescan,
            ImportExternalFiles,
            SyncDerivadas,
            CompactDatabase,
        };

        WorkflowCommandRunner runner_;
        QString lastMessage_;
        Operation operation_{Operation::Rescan};
        bool lastSucceeded_{false};
        bool lastWarning_{false};
        bool running_{false};
    };

} // namespace ssa::presentation
