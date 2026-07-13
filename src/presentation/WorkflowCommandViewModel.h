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
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)
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
        [[nodiscard]] bool samRefreshEnabled() const;
        [[nodiscard]] bool samAutoRefreshEnabled() const;
        [[nodiscard]] int samIntervalMinutes() const;
        [[nodiscard]] QString samScrapReportRoot() const;
        [[nodiscard]] QString samCaFile() const;
        [[nodiscard]] QString samBaseUrl() const;
        [[nodiscard]] QString samExecutorSectors() const;
        [[nodiscard]] QString samScope() const;
        void applyPreferences(const ports::UserPreferencesSnapshot& snapshot);
        void writePreferences(ports::UserPreferencesSnapshot& snapshot) const;

      signals:
        void lastResultChanged();
        void runningChanged();
        void samRefreshSettingsChanged();
        void preferencesSaveRequested();

      public slots:
        void importExternalFiles(const QVariantList& selectedFiles);
        void rescanIncremental();
        void rescanFull();
        void syncDerivadas();
        void compactDatabase();
        void refreshSamNow();
        void setSamRefreshEnabled(bool enabled);
        void setSamAutoRefreshEnabled(bool enabled);
        void setSamIntervalMinutes(int minutes);
        void setSamScrapReportRoot(const QString& path);
        void setSamCaFile(const QString& path);
        void setSamBaseUrl(const QString& url);
        void setSamExecutorSectors(const QString& sectors);
        void setSamScope(const QString& scope);

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
        void syncSamRefreshTimer();
        void setSamTextSetting(QString& target, const QString& value);
        [[nodiscard]] ports::SamRefreshRequest samRefreshRequest() const;
        [[nodiscard]] OperationMessages messagesForCurrentOperation() const;

        enum class Operation {
            Rescan,
            ImportExternalFiles,
            SyncDerivadas,
            CompactDatabase,
            SamRefresh,
        };

        WorkflowCommandRunner runner_;
        QString lastMessage_;
        Operation operation_{Operation::Rescan};
        bool lastSucceeded_{false};
        bool lastWarning_{false};
        bool running_{false};
        QTimer samRefreshTimer_;
        QString samScrapReportRoot_;
        QString samCaFile_;
        QString samBaseUrl_;
        QString samExecutorSectors_;
        QString samScope_{QStringLiteral("consulta")};
        int samIntervalMinutes_{30'000};
        bool samRefreshEnabled_{false};
        bool samAutoRefreshEnabled_{false};
    };

} // namespace ssa::presentation
