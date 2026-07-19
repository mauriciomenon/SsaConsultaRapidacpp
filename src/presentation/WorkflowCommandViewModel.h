#pragma once

#include "application/SsaWorkflowService.h"
#include "ports/IUserPreferencesStore.h"
#include "presentation/WorkflowCommandRunner.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <memory>

namespace ssa::presentation {

    class WorkflowCommandViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastSucceeded READ lastSucceeded NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastWarning READ lastWarning NOTIFY lastResultChanged)
        Q_PROPERTY(bool lastCanceled READ lastCanceled NOTIFY lastResultChanged)
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)
        Q_PROPERTY(bool canceling READ canceling NOTIFY stateChanged)
        Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)
        Q_PROPERTY(bool samRefreshEnabled READ samRefreshEnabled WRITE setSamRefreshEnabled NOTIFY
                       samRefreshSettingsChanged)
        Q_PROPERTY(bool samAutoRefreshEnabled READ samAutoRefreshEnabled WRITE
                       setSamAutoRefreshEnabled NOTIFY samRefreshSettingsChanged)
        Q_PROPERTY(int samIntervalMinutes READ samIntervalMinutes WRITE setSamIntervalMinutes NOTIFY
                       samRefreshSettingsChanged)
        Q_PROPERTY(QString samScrapReportRoot READ samScrapReportRoot WRITE setSamScrapReportRoot
                       NOTIFY samRefreshSettingsChanged)
        Q_PROPERTY(
            QString samCaFile READ samCaFile WRITE setSamCaFile NOTIFY samRefreshSettingsChanged)
        Q_PROPERTY(
            QString samBaseUrl READ samBaseUrl WRITE setSamBaseUrl NOTIFY samRefreshSettingsChanged)
        Q_PROPERTY(QString samExecutorSectors READ samExecutorSectors WRITE setSamExecutorSectors
                       NOTIFY samRefreshSettingsChanged)
        Q_PROPERTY(
            QString samScope READ samScope WRITE setSamScope NOTIFY samRefreshSettingsChanged)
        Q_PROPERTY(int importRowsPerChunk READ importRowsPerChunk WRITE setImportRowsPerChunk NOTIFY
                       importExecutionSettingsChanged)
        Q_PROPERTY(int importSqliteBusyWaitMs READ importSqliteBusyWaitMs WRITE
                       setImportSqliteBusyWaitMs NOTIFY importExecutionSettingsChanged)

      public:
        explicit WorkflowCommandViewModel(
            std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent = nullptr);

        [[nodiscard]] QString lastMessage() const;
        [[nodiscard]] bool lastSucceeded() const;
        [[nodiscard]] bool lastWarning() const;
        [[nodiscard]] bool lastCanceled() const;
        [[nodiscard]] bool running() const;
        [[nodiscard]] bool canceling() const;
        [[nodiscard]] bool canCancel() const;
        Q_INVOKABLE [[nodiscard]] bool legacyDerivadasConverterAvailable() const;
        [[nodiscard]] QString runningMessage() const;
        [[nodiscard]] QString successMessage() const;
        [[nodiscard]] QString failureMessage() const;
        [[nodiscard]] bool samRefreshEnabled() const;
        [[nodiscard]] bool samAutoRefreshEnabled() const;
        [[nodiscard]] int samIntervalMinutes() const;
        [[nodiscard]] QString samScrapReportRoot() const;
        [[nodiscard]] QString samCaFile() const;
        [[nodiscard]] QString samBaseUrl() const;
        [[nodiscard]] QString samExecutorSectors() const;
        [[nodiscard]] QString samScope() const;
        [[nodiscard]] int importRowsPerChunk() const;
        [[nodiscard]] int importSqliteBusyWaitMs() const;
        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;

      signals:
        void lastResultChanged();
        void runningChanged();
        void stateChanged();
        void samRefreshSettingsChanged();
        void importExecutionSettingsChanged();
        void preferencesSaveRequested();
        void logEntryRequested(const QString& severity, const QString& source,
                               const QString& message, const QString& detail);

      public slots:
        void importExternalFiles(const QVariantList& selectedFiles);
        void importDerivations(const QVariantList& selectedFiles);
        void rescanIncremental();
        void rescanFull();
        void cleanOrphanDerivations();
        void compactDatabase();
        void refreshSamNow();
        void cancel();
        void setSamRefreshEnabled(bool enabled);
        void setSamAutoRefreshEnabled(bool enabled);
        void setSamIntervalMinutes(int minutes);
        void setSamScrapReportRoot(const QString& path);
        void setSamCaFile(const QString& path);
        void setSamBaseUrl(const QString& url);
        void setSamExecutorSectors(const QString& sectors);
        void setSamScope(const QString& scope);
        void setImportRowsPerChunk(int rows);
        void setImportSqliteBusyWaitMs(int milliseconds);

      private:
        struct OperationMessages {
            QString running;
            QString success;
            QString failure;
            QString canceled;
        };

        void startRescan(ports::RescanMode mode);
        void applyResult(const ports::WorkflowResult& result);
        void handleRunnerStateChanged(WorkflowCommandRunner::State state);
        void setResult(QString message, bool succeeded, bool warning = false,
                       bool canceled = false);
        void syncSamRefreshTimer();
        void setSamTextSetting(QString& target, const QString& value);
        [[nodiscard]] ports::SamRefreshRequest samRefreshRequest() const;
        [[nodiscard]] ports::ImportExecutionOptions importExecutionOptions() const;
        [[nodiscard]] OperationMessages messagesForCurrentOperation() const;

        enum class Operation {
            Rescan,
            ImportExternalFiles,
            ImportDerivations,
            CleanOrphanDerivations,
            CompactDatabase,
            SamRefresh,
        };

        WorkflowCommandRunner runner_;
        QString lastMessage_;
        Operation operation_{Operation::Rescan};
        bool lastSucceeded_{false};
        bool lastWarning_{false};
        bool lastCanceled_{false};
        bool running_{false};
        bool canceling_{false};
        QTimer samRefreshTimer_;
        QString samScrapReportRoot_;
        QString samCaFile_;
        QString samBaseUrl_;
        QString samExecutorSectors_;
        QString samScope_{QStringLiteral("consulta")};
        int samIntervalMinutes_{30'000};
        bool samRefreshEnabled_{false};
        bool samAutoRefreshEnabled_{false};
        int importRowsPerChunk_{ports::ImportExecutionPreferencesSnapshot::kDefaultRowsPerChunk};
        int importSqliteBusyWaitMs_{
            ports::ImportExecutionPreferencesSnapshot::kDefaultSqliteBusyWaitMs};
    };

} // namespace ssa::presentation
