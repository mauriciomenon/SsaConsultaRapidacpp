#include "presentation/UserPreferencesCoordinator.h"

#include <QDebug>
#include <QThread>
#include <QtConcurrent>

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

    void UserPreferencesCoordinator::shutdown(
        std::optional<ports::UserPreferencesSnapshot> finalSnapshot) {
        if (shuttingDown_) {
            return;
        }
        if (!finalSnapshot && hasPendingSnapshot_) {
            finalSnapshot = std::move(pendingSnapshot_);
        }
        shuttingDown_ = true;
        saveTimer_.stop();
        disconnect(&saveTimer_, nullptr, this, nullptr);
        disconnect(&watcher_, nullptr, this, nullptr);
        snapshotProvider_ = nullptr;
        hasPendingSnapshot_ = false;
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
        }
        activeTask_.reset();
        if (!preferencesStore_ || !finalSnapshot) {
            return;
        }
        try {
            preferencesStore_->save(*finalSnapshot);
        } catch (const std::exception& exc) {
            qWarning() << "Failed to save final preferences snapshot:" << exc.what();
        } catch (...) {
            qWarning() << "Failed to save final preferences snapshot: unknown error";
        }
    }

    ports::UserPreferencesSnapshot UserPreferencesCoordinator::loadInitial() const {
        if (!preferencesStore_) {
            return {};
        }
        return preferencesStore_->load();
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

        const auto store = preferencesStore_;
        auto snapshot = std::move(pendingSnapshot_);
        hasPendingSnapshot_ = false;
        const auto task = std::make_shared<SaveTaskState>();
        activeTask_ = task;
        watcher_.setFuture(QtConcurrent::run([store, snapshot = std::move(snapshot), task] {
            std::string error;
            try {
                store->save(snapshot);
            } catch (const std::exception& exc) {
                error = exc.what();
            } catch (...) {
                error = "erro desconhecido";
            }
            const std::scoped_lock lock(task->mutex);
            task->error = std::move(error);
        }));
    }

    void UserPreferencesCoordinator::finishSave() {
        if (!ensureOwnerThread("finishSave") || shuttingDown_) {
            return;
        }
        std::string error;
        const auto task = activeTask_;
        if (task) {
            const std::scoped_lock lock(task->mutex);
            error = task->error;
        }
        activeTask_.reset();
        if (error.empty()) {
            emit saved();
        } else {
            const auto message = QString::fromStdString(error);
            qWarning() << "Failed to save preferences:" << message;
            emit this->saveFailed(
                QStringLiteral("Falha interna ao salvar preferencias: %1").arg(message));
        }
        if (hasPendingSnapshot_ || snapshotProvider_) {
            flushPendingSave();
        }
    }

} // namespace ssa::presentation
