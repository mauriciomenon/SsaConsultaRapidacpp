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
        connect(&watcher_, &QFutureWatcher<bool>::finished, this,
                &UserPreferencesCoordinator::finishSave);
    }

    UserPreferencesCoordinator::~UserPreferencesCoordinator() {
        saveTimer_.stop();
        if (!preferencesStore_) {
            pendingSnapshotProvider_ = nullptr;
            hasPendingSnapshot_ = false;
            return;
        }
        if (watcher_.isRunning()) {
            watcher_.waitForFinished();
            try {
                watcher_.result();
            } catch (const std::exception& exc) {
                qWarning() << "Failed to save preferences during shutdown:" << exc.what();
            } catch (...) {
                qWarning() << "Failed to save preferences during shutdown";
            }
        }
        pendingSnapshotProvider_ = nullptr;
        hasPendingSnapshot_ = false;
    }

    ports::UserPreferencesSnapshot UserPreferencesCoordinator::loadInitial() const {
        if (!preferencesStore_) {
            return {};
        }
        return preferencesStore_->load();
    }

    void UserPreferencesCoordinator::scheduleSave(
        std::function<ports::UserPreferencesSnapshot()> snapshotProvider) {
        Q_ASSERT(thread() == QThread::currentThread());
        if (!preferencesStore_) {
            return;
        }
        pendingSnapshotProvider_ = std::move(snapshotProvider);
        saveTimer_.start();
    }

    void UserPreferencesCoordinator::saveNowOrSchedule(ports::UserPreferencesSnapshot snapshot) {
        Q_ASSERT(thread() == QThread::currentThread());
        if (!preferencesStore_) {
            emit saved();
            return;
        }
        pendingSnapshot_ = std::move(snapshot);
        hasPendingSnapshot_ = true;
        pendingSnapshotProvider_ = nullptr;
        if (watcher_.isRunning()) {
            return;
        }
        saveTimer_.stop();
        flushPendingSave();
    }

    void UserPreferencesCoordinator::flushPendingSave() {
        Q_ASSERT(thread() == QThread::currentThread());
        if (!preferencesStore_ || watcher_.isRunning()) {
            return;
        }
        if (pendingSnapshotProvider_) {
            pendingSnapshot_ = pendingSnapshotProvider_();
            pendingSnapshotProvider_ = nullptr;
            hasPendingSnapshot_ = true;
        }
        if (!hasPendingSnapshot_) {
            return;
        }
        if (watcher_.isRunning()) {
            return;
        }

        const auto store = preferencesStore_;
        auto snapshot = std::move(pendingSnapshot_);
        hasPendingSnapshot_ = false;

        watcher_.setFuture(QtConcurrent::run([store, snapshot = std::move(snapshot)] {
            store->save(snapshot);
            return true;
        }));
    }

    void UserPreferencesCoordinator::finishSave() {
        Q_ASSERT(thread() == QThread::currentThread());
        try {
            watcher_.result();
            emit saved();
        } catch (const std::exception& exc) {
            (void)exc;
            emit saveFailed("Falha interna ao salvar preferencias");
        }
        if (hasPendingSnapshot_ || pendingSnapshotProvider_) {
            flushPendingSave();
        }
    }

} // namespace ssa::presentation
