#include "presentation/WorkflowCommandViewModel.h"

#include "qt/FilesystemPath.h"

#include <QLocale>
#include <QStringList>
#include <QUrl>

#include <algorithm>
#include <utility>
#include <vector>

namespace ssa::presentation {

    namespace {

        enum class FileSelectionError { None, EmptySelection, NonLocalFile };

        constexpr int kMaxProgressErrorLines = 8;

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

        QString localizedNumber(const std::size_t value) {
            static const QLocale locale{QLocale::Portuguese, QLocale::Brazil};
            return locale.toString(static_cast<qulonglong>(value));
        }

        QString counted(const std::size_t value, const QString& singular, const QString& plural) {
            return localizedNumber(value) + QStringLiteral(" ") + (value == 1 ? singular : plural);
        }

        QString workflowDetail(const std::string& detail) {
            if (detail == "consolidation canceled") {
                return QStringLiteral("consolidacao cancelada");
            }
            return QString::fromStdString(detail);
        }

        QString importResultMessage(const ports::WorkflowResult& result, QString prefix,
                                    const bool hideTechnicalWithoutSummary) {
            if (!result.importSummary.has_value()) {
                if (hideTechnicalWithoutSummary && result.ok() && result.warning &&
                    !result.message.empty()) {
                    return prefix + QStringLiteral(" com avisos: ") +
                           workflowDetail(result.message);
                }
                if (hideTechnicalWithoutSummary || result.message.empty()) {
                    return prefix;
                }
                return QString::fromStdString(result.message);
            }
            if (result.ok() && result.warning) {
                prefix += QStringLiteral(" com avisos");
            }
            const auto& summary = *result.importSummary;
            QStringList facts{
                counted(summary.discovered, QStringLiteral("arquivo examinado"),
                        QStringLiteral("arquivos examinados")),
                counted(summary.accepted, QStringLiteral("aplicado"), QStringLiteral("aplicados"))};
            if (summary.ignored > 0) {
                facts.push_back(counted(summary.ignored, QStringLiteral("ignorado"),
                                        QStringLiteral("ignorados")));
            }
            if (summary.rejected > 0) {
                facts.push_back(counted(summary.rejected, QStringLiteral("rejeitado"),
                                        QStringLiteral("rejeitados")));
            }
            if (summary.failed > 0) {
                facts.push_back(
                    counted(summary.failed, QStringLiteral("falhou"), QStringLiteral("falharam")));
            }
            if (summary.inserts > 0) {
                facts.push_back(counted(summary.inserts, QStringLiteral("SSA inserida"),
                                        QStringLiteral("SSAs inseridas")));
            }
            if (summary.updates > 0) {
                facts.push_back(counted(summary.updates, QStringLiteral("SSA atualizada"),
                                        QStringLiteral("SSAs atualizadas")));
            }
            if (summary.failed == 0 && summary.rejected == 0 && summary.invalidRows == 0 &&
                summary.conflicts == 0) {
                facts.push_back(QStringLiteral("nenhuma falha"));
            }
            return prefix + QStringLiteral(": ") + facts.join(QStringLiteral("; "));
        }

        QString fileReason(const std::string& reason) {
            if (reason == "header_not_recognized") {
                return QStringLiteral("cabecalho SSA nao reconhecido; planilha ignorada");
            }
            if (reason == "required_columns_missing") {
                return QStringLiteral("colunas obrigatorias ausentes");
            }
            if (reason == "ambiguous_headers") {
                return QStringLiteral("cabecalhos ambiguos");
            }
            if (reason == "duplicate_conflict") {
                return QStringLiteral("conflito entre linhas duplicadas");
            }
            if (reason == "invalid_rows") {
                return QStringLiteral("linhas invalidas");
            }
            if (reason == "no_valid_rows") {
                return QStringLiteral("nenhuma linha valida");
            }
            if (reason == "sam_rejected") {
                return QStringLiteral("arquivo rejeitado pelo SAM");
            }
            if (reason == "batch_rejected") {
                return QStringLiteral("lote revertido por rejeicao de outro arquivo");
            }
            if (reason == "operation_failed") {
                return QStringLiteral("falha operacional");
            }
            if (reason == "canceled") {
                return QStringLiteral("operacao cancelada");
            }
            return QStringLiteral("motivo disponivel no log tecnico");
        }

        QString appliedFileMessage(const ports::ImportFileResult& file) {
            QStringList facts;
            if (file.inserts > 0) {
                facts.push_back(counted(file.inserts, QStringLiteral("SSA nova"),
                                        QStringLiteral("SSAs novas")));
            }
            if (file.updates > 0) {
                facts.push_back(counted(file.updates, QStringLiteral("SSA atualizada"),
                                        QStringLiteral("SSAs atualizadas")));
            }
            if (file.unchangedRows > 0) {
                facts.push_back(counted(file.unchangedRows, QStringLiteral("SSA sem alteracao"),
                                        QStringLiteral("SSAs sem alteracao")));
            }
            if (facts.empty()) {
                facts.push_back(QStringLiteral("sem alteracoes"));
            }
            return QString::fromStdString(file.source) + QStringLiteral(": ") +
                   facts.join(QStringLiteral("; "));
        }

    } // namespace

    WorkflowCommandViewModel::WorkflowCommandViewModel(
        std::shared_ptr<application::SsaWorkflowService> workflows, QObject* parent)
        : QObject(parent), runner_(std::move(workflows), this), samRefreshTimer_(this) {
        connect(&runner_, &WorkflowCommandRunner::stateChanged, this,
                &WorkflowCommandViewModel::handleRunnerStateChanged);
        connect(&runner_, &WorkflowCommandRunner::finished, this,
                &WorkflowCommandViewModel::applyResult);
        connect(&runner_, &WorkflowCommandRunner::progressReported, this,
                &WorkflowCommandViewModel::handleProgress);
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
        startProgressSession(tr("Importacao em andamento"));
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
        startProgressSession(tr("Importacao de derivadas em andamento"));
        runner_.importDerivations(files, importExecutionOptions());
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
        request.rowsPerChunk = static_cast<std::size_t>(importRowsPerChunk_);
        request.sqliteBusyWait = std::chrono::milliseconds{importSqliteBusyWaitMs_};
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
        startProgressSession(tr("Reescaneamento em andamento"));
        runner_.rescan(mode, importExecutionOptions());
    }

    void WorkflowCommandViewModel::startProgressSession(const QString& operationLabel) {
        progressSessionActive_ = true;
        progressPercentage_ = 0;
        progressCurrentFile_ = 0;
        progressTotalFiles_ = 0;
        progressFileName_.clear();
        emit progressSessionStarted(operationLabel);
    }

    void WorkflowCommandViewModel::handleProgress(const ports::WorkflowProgress& progress) {
        if (!progressSessionActive_) {
            return;
        }
        progressPercentage_ = progress.percentage;
        progressCurrentFile_ = progress.currentFile;
        progressTotalFiles_ = progress.totalFiles;
        progressFileName_ = QString::fromStdString(progress.fileName);
        if (progress.stage == ports::WorkflowProgressStage::Completed) {
            return;
        }
        const auto status = QString::fromStdString(progress.status);
        emit progressChanged(progressPercentage_, status, static_cast<int>(progressCurrentFile_),
                             static_cast<int>(progressTotalFiles_), progressFileName_);
        auto line = status;
        const auto detail = QString::fromStdString(progress.detail);
        if (!detail.isEmpty()) {
            line = line.isEmpty() ? detail : line + QStringLiteral(": ") + detail;
        }
        if (line.isEmpty()) {
            return;
        }
        if (progress.level == ports::WorkflowProgressLevel::Information) {
            emit progressOutputLine(line);
        } else {
            emit progressErrorLine(line);
        }
    }

    void WorkflowCommandViewModel::finishProgressSession(const ports::WorkflowResult& result,
                                                         const QString& message,
                                                         const bool canceled) {
        if (!progressSessionActive_) {
            return;
        }
        progressSessionActive_ = false;
        if (result.ok()) {
            progressPercentage_ = 100;
        }
        emit progressChanged(progressPercentage_, message, static_cast<int>(progressCurrentFile_),
                             static_cast<int>(progressTotalFiles_), progressFileName_);
        bool terminalDetailEmitted = false;
        QStringList deferredErrorLines;
        std::size_t ignoredCount = 0;
        std::size_t rejectedCount = 0;
        std::size_t failedCount = 0;
        QString primaryCause;
        if (result.importSummary.has_value()) {
            for (const auto& file : result.importSummary->files) {
                if (file.status == ports::ImportFileStatus::Applied ||
                    file.status == ports::ImportFileStatus::NoChanges) {
                    emit progressOutputLine(appliedFileMessage(file));
                    continue;
                }
                if (file.status == ports::ImportFileStatus::Ignored ||
                    file.status == ports::ImportFileStatus::Rejected ||
                    file.status == ports::ImportFileStatus::Failed) {
                    const auto reason = fileReason(file.reason);
                    const auto line =
                        QString::fromStdString(file.source) +
                        (reason.isEmpty() ? QString{} : QStringLiteral(" - ") + reason);
                    if (file.status == ports::ImportFileStatus::Ignored) {
                        ++ignoredCount;
                    } else if (file.status == ports::ImportFileStatus::Rejected) {
                        ++rejectedCount;
                    } else {
                        ++failedCount;
                    }
                    if (primaryCause.isEmpty() && file.status != ports::ImportFileStatus::Ignored) {
                        primaryCause = line;
                    }
                    deferredErrorLines.push_back(line);
                    terminalDetailEmitted = true;
                }
            }
        }
        if (deferredErrorLines.size() > static_cast<std::size_t>(kMaxProgressErrorLines)) {
            QStringList summaryParts;
            if (rejectedCount > 0) {
                summaryParts.push_back(counted(rejectedCount, QStringLiteral("rejeitado"),
                                               QStringLiteral("rejeitados")));
            }
            if (failedCount > 0) {
                summaryParts.push_back(
                    counted(failedCount, QStringLiteral("falhou"), QStringLiteral("falharam")));
            }
            if (ignoredCount > 0) {
                summaryParts.push_back(counted(ignoredCount, QStringLiteral("ignorado"),
                                               QStringLiteral("ignorados")));
            }
            const auto hiddenCount =
                static_cast<int>(deferredErrorLines.size()) - kMaxProgressErrorLines;
            emit progressErrorLine(QStringLiteral("Resumo: ") + summaryParts.join(QStringLiteral("; ")));
            if (!primaryCause.isEmpty()) {
                emit progressErrorLine(QStringLiteral("Causa principal: ") + primaryCause);
            }
            for (int index = 0; index < kMaxProgressErrorLines; ++index) {
                emit progressErrorLine(deferredErrorLines.at(index));
            }
            emit progressErrorLine(
                counted(static_cast<std::size_t>(hiddenCount), QStringLiteral("arquivo oculto"),
                        QStringLiteral("arquivos ocultos")) +
                QStringLiteral("; detalhes completos no log tecnico"));
        } else {
            for (const auto& line : deferredErrorLines) {
                emit progressErrorLine(line);
            }
        }
        const auto diagnostic = QString::fromStdString(result.diagnostic);
        if (!diagnostic.isEmpty()) {
            emit progressErrorLine(diagnostic);
            terminalDetailEmitted = true;
        }
        if (!terminalDetailEmitted && (!result.ok() || canceled || result.warning)) {
            emit progressErrorLine(message);
        }
        const auto messages = messagesForCurrentOperation();
        const auto title =
            canceled ? messages.canceled
            : result.ok()
                ? messages.success + (result.warning ? QStringLiteral(" com avisos") : QString{})
                : messages.failure;
        emit progressSessionFinished(result.ok(), canceled, title, message);
    }

    void WorkflowCommandViewModel::applyResult(const ports::WorkflowResult& result) {
        if (result.status == ports::WorkflowStatus::Canceled) {
            const auto message = messagesForCurrentOperation().canceled;
            finishProgressSession(result, message, true);
            emit logEntryRequested(QStringLiteral("Warning"), QStringLiteral("Workflow"), message,
                                   QString::fromStdString(result.diagnostic));
            setResult(message, false, false, true);
            return;
        }
        if (result.ok()) {
            const auto message =
                importResultMessage(result, successMessage(), progressSessionActive_);
            finishProgressSession(result, message, false);
            emit logEntryRequested(
                result.warning ? QStringLiteral("Warning") : QStringLiteral("Info"),
                QStringLiteral("Workflow"), QString::fromStdString(result.message),
                QString::fromStdString(result.diagnostic));
            setResult(message, true, result.warning);
            return;
        }
        const auto message = importResultMessage(result, failureMessage(), progressSessionActive_);
        finishProgressSession(result, message, false);
        emit logEntryRequested(QStringLiteral("Error"), QStringLiteral("Workflow"),
                               QString::fromStdString(result.message),
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
