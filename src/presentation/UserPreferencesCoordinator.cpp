#include "presentation/UserPreferencesCoordinator.h"

#include <QDebug>
#include <QThread>
#include <QtConcurrentRun>

#include <system_error>
#include <utility>

namespace ssa::presentation {

    UserPreferencesCoordinator::UserPreferencesCoordinator(
        std::shared_ptr<ports::IUserPreferencesStore> preferencesStore, QObject* parent)
        : QObject(parent), preferencesStore_(std::move(preferencesStore)) {
        saveTimer_.setSingleShot(true);
        saveTimer_.setInterval(500);
        connect(&saveTimer_, &QTimer::timeout, this, &UserPreferencesCoordinator::flushPendingSave);
        connect(&watcher_, &QFutureWatcher<void>::finished, this,
                &UserPreferencesCoordinator::finishSave);
    }

    UserPreferencesCoordinator::~UserPreferencesCoordinator() {
        shutdown();
    }

    void UserPreferencesCoordinator::shutdown() {
        if (shuttingDown_) {
            disconnect(&watcher_, nullptr, this, nullptr);
            return;
        }
        shuttingDown_ = true;
        emit shutdownStarted();
        saveTimer_.stop();
        disconnect(&saveTimer_, nullptr, this, nullptr);
        disconnect(&watcher_, nullptr, this, nullptr);
        snapshotProvider_ = nullptr;
        hasPendingSnapshot_ = false;
        finalSnapshot_.reset();
        cancel();
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
        }
    }

    void UserPreferencesCoordinator::beginShutdown(
        std::optional<ports::UserPreferencesSnapshot> finalSnapshot) {
        if (shuttingDown_) {
            return;
        }
        shuttingDown_ = true;
        emit shutdownStarted();
        saveTimer_.stop();
        disconnect(&saveTimer_, nullptr, this, nullptr);
        snapshotProvider_ = nullptr;
        hasPendingSnapshot_ = false;
        finalSnapshot_ = std::move(finalSnapshot);
        if (running_) {
            cancel();
            return;
        }
        if (!preferencesStore_ || !finalSnapshot_) {
            finishShutdown();
            return;
        }
        auto snapshot = std::move(*finalSnapshot_);
        finalSnapshot_.reset();
        startSave(std::move(snapshot), true);
    }

    ports::UserPreferencesSnapshot UserPreferencesCoordinator::loadInitial() const {
        if (!preferencesStore_) {
            return {};
        }
        return preferencesStore_->load();
    }

    bool UserPreferencesCoordinator::running() const {
        return running_;
    }

    bool UserPreferencesCoordinator::canceling() const {
        return canceling_;
    }

    bool UserPreferencesCoordinator::canCancel() const {
        return running_ && !canceling_;
    }

    void UserPreferencesCoordinator::cancel() {
        if (!canCancel()) {
            return;
        }
        canceling_ = true;
        activeStopSource_.request_stop();
        emit stateChanged();
    }

    bool UserPreferencesCoordinator::ensureOwnerThread(const char* operation) {
        if (thread() == QThread::currentThread()) {
            return true;
        }
        qWarning() << "UserPreferencesCoordinator" << operation << "called outside owner thread";
        emit this->saveFailed(
            QStringLiteral("Falha interna ao salvar preferencias: chamada fora da thread da GUI"));
        return false;
    }

    void UserPreferencesCoordinator::scheduleSave(
        std::function<ports::UserPreferencesSnapshot()> snapshotProvider) {
        if (!ensureOwnerThread("scheduleSave")) {
            return;
        }
        if (!preferencesStore_ || shuttingDown_) {
            return;
        }
        snapshotProvider_ = std::move(snapshotProvider);
        saveTimer_.start();
    }

    void UserPreferencesCoordinator::saveNowOrSchedule(ports::UserPreferencesSnapshot snapshot) {
        if (!ensureOwnerThread("saveNowOrSchedule")) {
            return;
        }
        if (shuttingDown_) {
            return;
        }
        if (!preferencesStore_) {
            emit saved();
            return;
        }
        pendingSnapshot_ = std::move(snapshot);
        hasPendingSnapshot_ = true;
        snapshotProvider_ = nullptr;
        if (watcher_.isRunning()) {
            return;
        }
        saveTimer_.stop();
        flushPendingSave();
    }

    void UserPreferencesCoordinator::flushPendingSave() {
        if (!ensureOwnerThread("flushPendingSave")) {
            return;
        }
        if (!preferencesStore_ || shuttingDown_ || watcher_.isRunning()) {
            return;
        }
        if (snapshotProvider_) {
            pendingSnapshot_ = snapshotProvider_();
            snapshotProvider_ = nullptr;
            hasPendingSnapshot_ = true;
        }
        if (!hasPendingSnapshot_) {
            return;
        }

        auto snapshot = std::move(pendingSnapshot_);
        hasPendingSnapshot_ = false;
        startSave(std::move(snapshot), false);
    }

    void UserPreferencesCoordinator::startSave(ports::UserPreferencesSnapshot snapshot,
                                               const bool finalSave) {
        const auto store = preferencesStore_;
        const auto task = std::make_shared<SaveTaskState>();
        task->finalSave = finalSave;
        activeTask_ = task;
        activeStopSource_ = std::stop_source{};
        const auto stopToken = activeStopSource_.get_token();
        running_ = true;
        canceling_ = false;
        emit stateChanged();
        watcher_.setFuture(
            QtConcurrent::run([store, snapshot = std::move(snapshot), task, stopToken] {
                std::string error;
                bool canceled = false;
                try {
                    store->save(snapshot, stopToken);
                } catch (const std::system_error& exception) {
                    if (exception.code() == std::errc::operation_canceled) {
                        canceled = true;
                    } else {
                        error = exception.what();
                    }
                } catch (const std::exception& exc) {
                    error = exc.what();
                } catch (...) {
                    error = "erro desconhecido";
                }
                const std::scoped_lock lock(task->mutex);
                task->error = std::move(error);
                task->canceled = canceled;
            }));
    }

    void UserPreferencesCoordinator::finishSave() {
        if (!ensureOwnerThread("finishSave")) {
            return;
        }
        std::string error;
        bool canceled = false;
        bool finalSave = false;
        const auto task = activeTask_;
        if (task) {
            const std::scoped_lock lock(task->mutex);
            error = task->error;
            canceled = task->canceled;
            finalSave = task->finalSave;
        }
        activeTask_.reset();
        running_ = false;
        canceling_ = false;
        emit stateChanged();
        if (!canceled && error.empty() && !shuttingDown_) {
            emit saved();
        } else if (!canceled && !error.empty()) {
            const auto message = QString::fromStdString(error);
            qWarning() << "Failed to save preferences:" << message;
            if (!shuttingDown_) {
                emit this->saveFailed(QStringLiteral("Falha ao salvar preferencias"));
            }
        }
        if (shuttingDown_) {
            if (!finalSave && finalSnapshot_ && preferencesStore_) {
                auto snapshot = std::move(*finalSnapshot_);
                finalSnapshot_.reset();
                startSave(std::move(snapshot), true);
                return;
            }
            finishShutdown();
            return;
        }
        if (hasPendingSnapshot_ || snapshotProvider_) {
            flushPendingSave();
        }
    }

    void UserPreferencesCoordinator::finishShutdown() {
        if (shutdownFinished_) {
            return;
        }
        shutdownFinished_ = true;
        emit shutdownFinished();
        emit stateChanged();
    }

} // namespace ssa::presentation
