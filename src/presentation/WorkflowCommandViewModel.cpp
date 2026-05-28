#include "presentation/WorkflowCommandViewModel.h"

#include <QUrl>

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
        : QObject(parent), runner_(std::move(workflows), this) {
        connect(&runner_, &WorkflowCommandRunner::runningChanged, this,
                &WorkflowCommandViewModel::setRunning);
        connect(&runner_, &WorkflowCommandRunner::finished, this,
                &WorkflowCommandViewModel::applyResult);
    }

    QString WorkflowCommandViewModel::lastMessage() const {
        return lastMessage_;
    }

    bool WorkflowCommandViewModel::lastSucceeded() const {
        return lastSucceeded_;
    }

    bool WorkflowCommandViewModel::running() const {
        return running_;
    }

    WorkflowCommandViewModel::OperationMessages
    WorkflowCommandViewModel::messagesForCurrentOperation() const {
        switch (operation_) {
        case Operation::Rescan:
            return {tr("Reescaneando dados..."), tr("Reescaneamento concluido"),
                    tr("Falha ao reescanear dados")};
        case Operation::ImportExternalFiles:
            return {tr("Importando arquivos..."), tr("Importacao concluida"),
                    tr("Falha ao importar arquivos")};
        case Operation::SyncDerivadas:
            return {tr("Sincronizando derivadas..."), tr("Sincronizacao de derivadas concluida"),
                    tr("Falha ao sincronizar derivadas")};
        }
        return {tr("Reescaneando dados..."), tr("Reescaneamento concluido"),
                tr("Falha ao reescanear dados")};
    }

    QString WorkflowCommandViewModel::runningMessage() const {
        return messagesForCurrentOperation().running;
    }

    QString WorkflowCommandViewModel::successMessage() const {
        return messagesForCurrentOperation().success;
    }

    QString WorkflowCommandViewModel::failureMessage() const {
        return messagesForCurrentOperation().failure;
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
        runner_.importExternalFiles(std::move(files));
    }

    void WorkflowCommandViewModel::rescanIncremental() {
        startRescan(ports::RescanMode::Incremental);
    }

    void WorkflowCommandViewModel::rescanFull() {
        startRescan(ports::RescanMode::Full);
    }

    void WorkflowCommandViewModel::syncDerivadas() {
        if (runner_.running()) {
            return;
        }
        operation_ = Operation::SyncDerivadas;
        runner_.syncDerivadas();
    }

    void WorkflowCommandViewModel::startRescan(const ports::RescanMode mode) {
        if (runner_.running()) {
            return;
        }
        operation_ = Operation::Rescan;
        runner_.rescan(mode);
    }

    void WorkflowCommandViewModel::applyResult(const ports::WorkflowResult& result) {
        if (result.ok()) {
            setResult(QString::fromStdString(result.message.empty() ? successMessage().toStdString()
                                                                    : result.message),
                      true);
            return;
        }
        setResult(QString::fromStdString(result.message), false);
    }

    void WorkflowCommandViewModel::setRunning(const bool running) {
        if (running_ == running) {
            return;
        }
        running_ = running;
        emit runningChanged();
    }

    void WorkflowCommandViewModel::setResult(QString message, const bool succeeded) {
        lastMessage_ = std::move(message);
        lastSucceeded_ = succeeded;
        emit lastResultChanged();
    }

} // namespace ssa::presentation
