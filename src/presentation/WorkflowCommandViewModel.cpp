#include "presentation/WorkflowCommandViewModel.h"

#include <QUrl>

#include <filesystem>
#include <utility>
#include <vector>

namespace ssa::presentation {

    namespace {

        std::filesystem::path filesystemPathFromLocalFile(const QString& localFile) {
#ifdef _WIN32
            return std::filesystem::path{localFile.toStdWString()};
#else
            return std::filesystem::path{localFile.toStdString()};
#endif
        }

        std::vector<std::filesystem::path> localFilePathsFromUrls(const QVariantList& selectedFiles,
                                                                  QString& errorMessage) {
            std::vector<std::filesystem::path> files;
            files.reserve(static_cast<std::size_t>(selectedFiles.size()));
            for (const auto& selectedFile : selectedFiles) {
                const QUrl url = selectedFile.toUrl();
                if (!url.isLocalFile()) {
                    errorMessage = "import_external_files local_files_required";
                    return {};
                }
                files.push_back(filesystemPathFromLocalFile(url.toLocalFile()));
            }
            return files;
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

    QString WorkflowCommandViewModel::runningMessage() const {
        return operation_ == Operation::ImportExternalFiles ? tr("Importando arquivos...")
                                                            : tr("Reescaneando dados...");
    }

    QString WorkflowCommandViewModel::successMessage() const {
        return operation_ == Operation::ImportExternalFiles ? tr("Importacao concluida")
                                                            : tr("Reescaneamento concluido");
    }

    QString WorkflowCommandViewModel::failureMessage() const {
        return operation_ == Operation::ImportExternalFiles ? tr("Falha ao importar arquivos")
                                                            : tr("Falha ao reescanear dados");
    }

    void WorkflowCommandViewModel::importExternalFiles(const QVariantList& selectedFiles) {
        if (runner_.running()) {
            return;
        }
        QString errorMessage;
        auto files = localFilePathsFromUrls(selectedFiles, errorMessage);
        if (!errorMessage.isEmpty()) {
            setResult(std::move(errorMessage), false);
            return;
        }
        operation_ = Operation::ImportExternalFiles;
        runner_.importExternalFiles(std::move(files));
    }

    void WorkflowCommandViewModel::rescanIncremental() {
        startRescan(ports::RescanMode::Incremental);
    }

    void WorkflowCommandViewModel::rescanFull() {
        startRescan(ports::RescanMode::Full);
    }

    void WorkflowCommandViewModel::startRescan(const ports::RescanMode mode) {
        if (runner_.running()) {
            return;
        }
        operation_ = Operation::Rescan;
        runner_.rescan(mode);
    }

    void WorkflowCommandViewModel::applyResult(const ports::WorkflowResult& result) {
        setResult(result.ok() ? successMessage() : QString::fromStdString(result.message),
                  result.ok());
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
