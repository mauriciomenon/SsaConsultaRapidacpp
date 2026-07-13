#include "presentation/DatabaseSwitchViewModel.h"

#include "qt/FilesystemPath.h"

#include <QThreadPool>
#include <QtConcurrentRun>

#include <exception>
#include <utility>

namespace ssa::presentation {

    DatabaseSwitchViewModel::DatabaseSwitchViewModel(
        std::shared_ptr<ports::IDatabaseValidator> validator,
        std::shared_ptr<ports::IApplicationLauncher> launcher, QObject* parent)
        : QObject(parent), validator_(std::move(validator)), launcher_(std::move(launcher)) {
        connect(&watcher_, &QFutureWatcher<void>::finished, this,
                &DatabaseSwitchViewModel::finishValidation);
    }

    DatabaseSwitchViewModel::~DatabaseSwitchViewModel() {
        shutdown();
    }

    bool DatabaseSwitchViewModel::running() const {
        return running_;
    }

    QString DatabaseSwitchViewModel::errorMessage() const {
        return errorMessage_;
    }

    void DatabaseSwitchViewModel::openDatabase(const QUrl& url) {
        if (running_ || shuttingDown_) {
            return;
        }
        if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
            setErrorMessage(QStringLiteral("Selecione um arquivo de banco local"));
            return;
        }
        if (!validator_ || !launcher_) {
            setErrorMessage(QStringLiteral("A troca de banco nao esta configurada"));
            return;
        }

        setErrorMessage({});
        pendingPath_ = qt::toFileSystemPath(url.toLocalFile());
        const auto state = std::make_shared<ValidationState>();
        validationState_ = state;
        const auto validator = validator_;
        const auto path = pendingPath_;
        validationStopSource_ = std::stop_source{};
        const auto stopToken = validationStopSource_.get_token();
        setRunning(true);
        watcher_.setFuture(
            QtConcurrent::run(QThreadPool::globalInstance(), [state, validator, path, stopToken] {
                try {
                    auto result = validator->validate(path, stopToken);
                    const std::scoped_lock lock(state->mutex);
                    state->result = std::move(result);
                } catch (...) {
                    const std::scoped_lock lock(state->mutex);
                    state->error = std::current_exception();
                }
            }));
    }

    void DatabaseSwitchViewModel::shutdown() {
        if (shuttingDown_) {
            return;
        }
        shuttingDown_ = true;
        disconnect(&watcher_, nullptr, this, nullptr);
        validationStopSource_.request_stop();
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
        }
        validationState_.reset();
        pendingPath_.clear();
        running_ = false;
    }

    void DatabaseSwitchViewModel::finishValidation() {
        std::optional<ports::DatabaseValidationResult> validation;
        std::exception_ptr error;
        if (validationState_) {
            const std::scoped_lock lock(validationState_->mutex);
            validation = std::move(validationState_->result);
            error = validationState_->error;
        }
        validationState_.reset();

        if (error) {
            setRunning(false);
            setErrorMessage(QStringLiteral("Falha ao validar o banco selecionado"));
            return;
        }
        if (!validation) {
            setRunning(false);
            setErrorMessage(QStringLiteral("A validacao do banco nao retornou resultado"));
            return;
        }
        if (!validation->valid) {
            setRunning(false);
            setErrorMessage(QString::fromStdString(validation->message));
            return;
        }

        const auto launch = launcher_->launchWithDatabase(pendingPath_);
        pendingPath_.clear();
        setRunning(false);
        if (!launch.started) {
            setErrorMessage(QString::fromStdString(launch.message));
            return;
        }
        emit replacementStarted();
    }

    void DatabaseSwitchViewModel::setRunning(const bool isRunning) {
        if (running_ == isRunning) {
            return;
        }
        running_ = isRunning;
        emit runningChanged();
    }

    void DatabaseSwitchViewModel::setErrorMessage(QString message) {
        if (errorMessage_ == message) {
            return;
        }
        errorMessage_ = std::move(message);
        emit errorMessageChanged();
    }

} // namespace ssa::presentation
