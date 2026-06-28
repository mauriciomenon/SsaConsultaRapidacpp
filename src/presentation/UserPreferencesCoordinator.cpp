#include "presentation/UserPreferencesCoordinator.h"

#include <QCoreApplication>
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
        saveTimer_.stop();
        if (!preferencesStore_) {
            snapshotProvider_ = nullptr;
            hasPendingSnapshot_ = false;
            return;
        }
        // Drain any in-flight save so it is not torn down concurrently. Using a
        // void future avoids the ResultStore<QString> race entirely; we still wait
        // for completion and pump events so the queued 'finished' signal is handled
        // before the watcher's destructor runs.
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
            QCoreApplication::processEvents();
        }
        snapshotProvider_ = nullptr;
        hasPendingSnapshot_ = false;
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
        if (!preferencesStore_) {
            return;
        }
        snapshotProvider_ = std::move(snapshotProvider);
        saveTimer_.start();
    }

    void UserPreferencesCoordinator::saveNowOrSchedule(ports::UserPreferencesSnapshot snapshot) {
        if (!ensureOwnerThread("saveNowOrSchedule")) {
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
        if (!preferencesStore_ || watcher_.isRunning()) {
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
        // Clear the previous error before launching the worker; do NOT hold the
        // mutex across setFuture or the worker would deadlock on completion.
        {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastSaveError_.clear();
        }

        watcher_.setFuture(QtConcurrent::run([store, snapshot = std::move(snapshot), this] {
            std::string error;
            try {
                store->save(snapshot);
            } catch (const std::exception& exc) {
                error = exc.what();
            } catch (...) {
                error = "erro desconhecido";
            }
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastSaveError_ = std::move(error);
        }));
    }

    void UserPreferencesCoordinator::finishSave() {
        if (!ensureOwnerThread("finishSave")) {
            return;
        }
        std::string error;
        {
            std::lock_guard<std::mutex> lock(errorMutex_);
            error = lastSaveError_;
        }
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
