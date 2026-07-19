#include "presentation/WorkflowCommandViewModel.h"

#include "qt/FilesystemPath.h"

#include <QUrl>

#include <algorithm>
#include <utility>
#include <vector>

namespace ssa::presentation {

    namespace {

        enum class FileSelectionError { None, EmptySelection, NonLocalFile };

        FileSelectionError localFilePathsFromUrls(const QVariantList& selectedFiles,
                                                  std::vector<QString>& files) {
            if (selectedFiles.empty()) {
                return FileSelectionError::EmptySelection;
            }
            files.reserve(static_cast<std::size_t>(selectedFiles.size()));
            for (const auto& selectedFile : selectedFiles) {
                const QUrl url = selectedFile.toUrl();
                if (!url.isLocalFile()) {
                    return FileSelectionError::NonLocalFile;
                }
                files.push_back(url.toLocalFile());
            }
            return FileSelectionError::None;
        }

    } // namespace

    WorkflowCommandViewModel::WorkflowCommandViewModel(
        std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent)
        : QObject(parent), runner_(std::move(workflows), this), samRefreshTimer_(this) {
        connect(&runner_, &WorkflowCommandRunner::stateChanged, this,
                &WorkflowCommandViewModel::handleRunnerStateChanged);
        connect(&runner_, &WorkflowCommandRunner::finished, this,
                &WorkflowCommandViewModel::applyResult);
        connect(&samRefreshTimer_, &QTimer::timeout, this,
                &WorkflowCommandViewModel::refreshSamNow);
    }

    QString WorkflowCommandViewModel::lastMessage() const {
        return lastMessage_;
    }

    bool WorkflowCommandViewModel::lastSucceeded() const {
        return lastSucceeded_;
    }

    bool WorkflowCommandViewModel::lastWarning() const {
        return lastWarning_;
    }

    bool WorkflowCommandViewModel::lastCanceled() const {
        return lastCanceled_;
    }

    bool WorkflowCommandViewModel::running() const {
        return running_;
    }

    bool WorkflowCommandViewModel::canceling() const {
        return canceling_;
    }

    bool WorkflowCommandViewModel::canCancel() const {
        return runner_.canCancel();
    }

    WorkflowCommandViewModel::OperationMessages
    WorkflowCommandViewModel::messagesForCurrentOperation() const {
        switch (operation_) {
        case Operation::Rescan:
            return {tr("Reescaneando dados..."), tr("Reescaneamento concluido"),
                    tr("Falha ao reescanear dados"), tr("Reescaneamento cancelado")};
        case Operation::ImportExternalFiles:
            return {tr("Importando arquivos..."), tr("Importacao concluida"),
                    tr("Falha ao importar arquivos"), tr("Importacao cancelada")};
        case Operation::ImportDerivations:
            return {tr("Importando derivadas..."), tr("Importacao de derivadas concluida"),
                    tr("Falha ao importar derivadas"), tr("Importacao de derivadas cancelada")};
        case Operation::CleanOrphanDerivations:
            return {tr("Limpando referencias orfas..."),
                    tr("Limpeza de referencias orfas concluida"),
                    tr("Falha ao limpar referencias orfas"), tr("Limpeza cancelada")};
        case Operation::CompactDatabase:
            return {tr("Compactando banco..."), tr("Banco compactado"),
                    tr("Falha ao compactar banco"), tr("Compactacao cancelada")};
        case Operation::SamRefresh:
            return {tr("Atualizando dados do SAM..."), tr("Atualizacao do SAM concluida"),
                    tr("Falha ao atualizar dados do SAM"), tr("Atualizacao do SAM cancelada")};
        }
        return {tr("Reescaneando dados..."), tr("Reescaneamento concluido"),
                tr("Falha ao reescanear dados"), tr("Reescaneamento cancelado")};
    }

    QString WorkflowCommandViewModel::runningMessage() const {
        return canceling_ ? tr("Cancelando...") : messagesForCurrentOperation().running;
    }

    QString WorkflowCommandViewModel::successMessage() const {
        return messagesForCurrentOperation().success;
    }

    QString WorkflowCommandViewModel::failureMessage() const {
        return messagesForCurrentOperation().failure;
    }

    bool WorkflowCommandViewModel::samRefreshEnabled() const {
        return samRefreshEnabled_;
    }

    bool WorkflowCommandViewModel::samAutoRefreshEnabled() const {
        return samAutoRefreshEnabled_;
    }

    int WorkflowCommandViewModel::samIntervalMinutes() const {
        return samIntervalMinutes_;
    }

    QString WorkflowCommandViewModel::samScrapReportRoot() const {
        return samScrapReportRoot_;
    }

    QString WorkflowCommandViewModel::samCaFile() const {
        return samCaFile_;
    }

    QString WorkflowCommandViewModel::samBaseUrl() const {
        return samBaseUrl_;
    }

    QString WorkflowCommandViewModel::samExecutorSectors() const {
        return samExecutorSectors_;
    }

    QString WorkflowCommandViewModel::samScope() const {
        return samScope_;
    }

    int WorkflowCommandViewModel::importRowsPerChunk() const {
        return importRowsPerChunk_;
    }

    int WorkflowCommandViewModel::importSqliteBusyWaitMs() const {
        return importSqliteBusyWaitMs_;
    }

    void
    WorkflowCommandViewModel::applyPreferences(const ports::UserPreferencesSnapshot& snapshot) {
        const auto& preferences = snapshot.samRefresh;
        samRefreshEnabled_ = preferences.enabled;
        samAutoRefreshEnabled_ = preferences.autoRefreshEnabled;
        samIntervalMinutes_ = std::clamp(preferences.intervalMinutes, 1, 30'000);
        samScrapReportRoot_ = QString::fromStdString(preferences.scrapReportRoot);
        samCaFile_ = QString::fromStdString(preferences.caFile);
        samBaseUrl_ = QString::fromStdString(preferences.baseUrl);
        samExecutorSectors_ = QString::fromStdString(preferences.executorSectors);
        samScope_ = QString::fromStdString(preferences.scope);
        importRowsPerChunk_ =
            std::clamp(snapshot.importExecution.rowsPerChunk, 1,
                       ports::ImportExecutionPreferencesSnapshot::kMaxRowsPerChunk);
        const auto busyWait =
            std::clamp(snapshot.importExecution.sqliteBusyWaitMs, 0,
                       ports::ImportExecutionPreferencesSnapshot::kMaxSqliteBusyWaitMs);
        importSqliteBusyWaitMs_ =
            busyWait -
            busyWait % ports::ImportExecutionPreferencesSnapshot::kSqliteBusyRetryGranularityMs;
        syncSamRefreshTimer();
        emit samRefreshSettingsChanged();
        emit importExecutionSettingsChanged();
    }

    void
    WorkflowCommandViewModel::writePreferences(ports::UserPreferencesSnapshot& snapshot) const {
        snapshot.samRefresh.enabled = samRefreshEnabled_;
        snapshot.samRefresh.autoRefreshEnabled = samAutoRefreshEnabled_;
        snapshot.samRefresh.intervalMinutes = samIntervalMinutes_;
        snapshot.samRefresh.scrapReportRoot = samScrapReportRoot_.toStdString();
        snapshot.samRefresh.caFile = samCaFile_.toStdString();
        snapshot.samRefresh.baseUrl = samBaseUrl_.toStdString();
        snapshot.samRefresh.executorSectors = samExecutorSectors_.toStdString();
        snapshot.samRefresh.scope = samScope_.toStdString();
        snapshot.importExecution.rowsPerChunk = importRowsPerChunk_;
        snapshot.importExecution.sqliteBusyWaitMs = importSqliteBusyWaitMs_;
    }

    void WorkflowCommandViewModel::importExternalFiles(const QVariantList& selectedFiles) {
        if (runner_.running()) {
            return;
        }

        operation_ = Operation::ImportExternalFiles;
        std::vector<QString> files;
        const auto parseError = localFilePathsFromUrls(selectedFiles, files);
        if (parseError == FileSelectionError::EmptySelection) {
            setResult(tr("Falha ao importar arquivos: nenhum arquivo selecionado"), false);
            return;
        }
        if (parseError == FileSelectionError::NonLocalFile) {
            setResult(tr("Falha ao importar arquivos: apenas arquivos locais sao suportados"),
                      false);
            return;
        }
        runner_.importExternalFiles(files, importExecutionOptions());
    }

    void WorkflowCommandViewModel::rescanIncremental() {
        startRescan(ports::RescanMode::Incremental);
    }

    void WorkflowCommandViewModel::importDerivations(const QVariantList& selectedFiles) {
        if (runner_.running()) {
            return;
        }

        operation_ = Operation::ImportDerivations;
        std::vector<QString> files;
        const auto parseError = localFilePathsFromUrls(selectedFiles, files);
        if (parseError == FileSelectionError::EmptySelection) {
            setResult(tr("Falha ao importar derivadas: nenhum arquivo selecionado"), false);
            return;
        }
        if (parseError == FileSelectionError::NonLocalFile) {
            setResult(tr("Falha ao importar derivadas: apenas arquivos locais sao suportados"),
                      false);
            return;
        }
        runner_.importDerivations(files);
    }

    bool WorkflowCommandViewModel::legacyDerivadasConverterAvailable() const {
        return runner_.legacySpreadsheetConverterAvailable();
    }

    void WorkflowCommandViewModel::rescanFull() {
        startRescan(ports::RescanMode::Full);
    }

    void WorkflowCommandViewModel::cleanOrphanDerivations() {
        if (runner_.running()) {
            return;
        }
        operation_ = Operation::CleanOrphanDerivations;
        runner_.cleanOrphanDerivations();
    }

    void WorkflowCommandViewModel::compactDatabase() {
        if (runner_.running()) {
            return;
        }
        operation_ = Operation::CompactDatabase;
        runner_.compactDatabase();
    }

    void WorkflowCommandViewModel::refreshSamNow() {
        if (runner_.running()) {
            return;
        }
        operation_ = Operation::SamRefresh;
        runner_.refreshSam(samRefreshRequest());
    }

    void WorkflowCommandViewModel::cancel() {
        runner_.cancel();
    }

    void WorkflowCommandViewModel::setSamRefreshEnabled(const bool enabled) {
        if (samRefreshEnabled_ == enabled) {
            return;
        }
        samRefreshEnabled_ = enabled;
        syncSamRefreshTimer();
        emit samRefreshSettingsChanged();
        emit preferencesSaveRequested();
    }

    void WorkflowCommandViewModel::setSamAutoRefreshEnabled(const bool enabled) {
        if (samAutoRefreshEnabled_ == enabled) {
            return;
        }
        samAutoRefreshEnabled_ = enabled;
        syncSamRefreshTimer();
        emit samRefreshSettingsChanged();
        emit preferencesSaveRequested();
    }

    void WorkflowCommandViewModel::setSamIntervalMinutes(const int minutes) {
        const auto value = std::clamp(minutes, 1, 30'000);
        if (samIntervalMinutes_ == value) {
            return;
        }
        samIntervalMinutes_ = value;
        syncSamRefreshTimer();
        emit samRefreshSettingsChanged();
        emit preferencesSaveRequested();
    }

    void WorkflowCommandViewModel::setSamTextSetting(QString& target, const QString& value) {
        if (target == value) {
            return;
        }
        target = value;
        emit samRefreshSettingsChanged();
        emit preferencesSaveRequested();
    }

    void WorkflowCommandViewModel::setSamScrapReportRoot(const QString& path) {
        setSamTextSetting(samScrapReportRoot_, path.trimmed());
    }

    void WorkflowCommandViewModel::setSamCaFile(const QString& path) {
        setSamTextSetting(samCaFile_, path.trimmed());
    }

    void WorkflowCommandViewModel::setSamBaseUrl(const QString& url) {
        setSamTextSetting(samBaseUrl_, url.trimmed());
    }

    void WorkflowCommandViewModel::setSamExecutorSectors(const QString& sectors) {
        setSamTextSetting(samExecutorSectors_, sectors.trimmed());
    }

    void WorkflowCommandViewModel::setSamScope(const QString& scope) {
        setSamTextSetting(samScope_, scope.trimmed());
    }

    void WorkflowCommandViewModel::setImportRowsPerChunk(const int rows) {
        const auto value =
            std::clamp(rows, 1, ports::ImportExecutionPreferencesSnapshot::kMaxRowsPerChunk);
        if (importRowsPerChunk_ == value) {
            return;
        }
        importRowsPerChunk_ = value;
        emit importExecutionSettingsChanged();
        emit preferencesSaveRequested();
    }

    void WorkflowCommandViewModel::setImportSqliteBusyWaitMs(const int milliseconds) {
        const auto clamped = std::clamp(
            milliseconds, 0, ports::ImportExecutionPreferencesSnapshot::kMaxSqliteBusyWaitMs);
        const auto value =
            clamped -
            clamped % ports::ImportExecutionPreferencesSnapshot::kSqliteBusyRetryGranularityMs;
        if (importSqliteBusyWaitMs_ == value) {
            return;
        }
        importSqliteBusyWaitMs_ = value;
        emit importExecutionSettingsChanged();
        emit preferencesSaveRequested();
    }

    void WorkflowCommandViewModel::syncSamRefreshTimer() {
        samRefreshTimer_.setInterval(samIntervalMinutes_ * 60'000);
        if (samRefreshEnabled_ && samAutoRefreshEnabled_) {
            samRefreshTimer_.start();
        } else {
            samRefreshTimer_.stop();
        }
    }

    ports::SamRefreshRequest WorkflowCommandViewModel::samRefreshRequest() const {
        ports::SamRefreshRequest request;
        request.enabled = samRefreshEnabled_;
        request.scrapReportRoot = qt::toFileSystemPath(samScrapReportRoot_);
        request.caFile = qt::toFileSystemPath(samCaFile_);
        request.baseUrl = samBaseUrl_.toStdString();
        const auto sectors = samExecutorSectors_.split(',', Qt::SkipEmptyParts);
        request.executorSectors.reserve(static_cast<std::size_t>(sectors.size()));
        for (const auto& sector : sectors) {
            request.executorSectors.push_back(sector.trimmed().toStdString());
        }
        request.scope = samScope_.toStdString();
        request.intervalMinutes = samIntervalMinutes_;
        return request;
    }

    ports::ImportExecutionOptions WorkflowCommandViewModel::importExecutionOptions() const {
        ports::ImportExecutionOptions options;
        options.rowsPerChunk = static_cast<std::size_t>(importRowsPerChunk_);
        options.sqliteBusyWait = std::chrono::milliseconds{importSqliteBusyWaitMs_};
        return options;
    }

    void WorkflowCommandViewModel::startRescan(const ports::RescanMode mode) {
        if (runner_.running()) {
            return;
        }
        operation_ = Operation::Rescan;
        runner_.rescan(mode, importExecutionOptions());
    }

    void WorkflowCommandViewModel::applyResult(const ports::WorkflowResult& result) {
        if (result.status == ports::WorkflowStatus::Canceled) {
            const auto message = messagesForCurrentOperation().canceled;
            emit logEntryRequested(QStringLiteral("Warning"), QStringLiteral("Workflow"), message,
                                   QString::fromStdString(result.diagnostic));
            setResult(message, false, false, true);
            return;
        }
        if (result.ok()) {
            const auto message = QString::fromStdString(
                result.message.empty() ? successMessage().toStdString() : result.message);
            emit logEntryRequested(
                result.warning ? QStringLiteral("Warning") : QStringLiteral("Info"),
                QStringLiteral("Workflow"), message, QString::fromStdString(result.diagnostic));
            setResult(message, true, result.warning);
            return;
        }
        const auto message = QString::fromStdString(result.message);
        emit logEntryRequested(QStringLiteral("Error"), QStringLiteral("Workflow"), message,
                               QString::fromStdString(result.diagnostic));
        setResult(message, false);
    }

    void
    WorkflowCommandViewModel::handleRunnerStateChanged(const WorkflowCommandRunner::State state) {
        const bool running = state != WorkflowCommandRunner::State::Idle;
        const bool canceling = state == WorkflowCommandRunner::State::Canceling;
        if (running_ != running) {
            running_ = running;
            emit runningChanged();
        }
        canceling_ = canceling;
        emit stateChanged();
    }

    void WorkflowCommandViewModel::setResult(QString message, const bool succeeded,
                                             const bool warning, const bool canceled) {
        lastMessage_ = std::move(message);
        lastSucceeded_ = succeeded;
        lastWarning_ = warning;
        lastCanceled_ = canceled;
        emit lastResultChanged();
    }

} // namespace ssa::presentation
