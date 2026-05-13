#include "presentation/UserPreferencesCoordinator.h"

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
        while (pendingSnapshotProvider_ || hasPendingSnapshot_ || watcher_.isRunning()) {
            if (watcher_.isRunning()) {
                watcher_.waitForFinished();
                continue;
            }
            flushPendingSave();
        }
    }

    ports::UserPreferencesSnapshot UserPreferencesCoordinator::loadInitial() const {
        if (!preferencesStore_) {
            return {};
        }
        return preferencesStore_->load();
    }

    void UserPreferencesCoordinator::scheduleSave(
        std::function<ports::UserPreferencesSnapshot()> snapshotProvider) {
        if (!preferencesStore_) {
            return;
        }
        pendingSnapshotProvider_ = std::move(snapshotProvider);
        saveTimer_.start();
    }

    void UserPreferencesCoordinator::saveNowOrSchedule(ports::UserPreferencesSnapshot snapshot) {
        if (!preferencesStore_) {
            emit saved();
            return;
        }
        pendingSnapshot_ = std::move(snapshot);
        hasPendingSnapshot_ = true;
        pendingSnapshotProvider_ = nullptr;
        if (watcher_.isRunning()) {
            if (!saveTimer_.isActive()) {
                saveTimer_.start();
            }
            return;
        }
        saveTimer_.stop();
        flushPendingSave();
    }

    void UserPreferencesCoordinator::flushPendingSave() {
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

        const auto store = preferencesStore_;
        auto snapshot = std::move(pendingSnapshot_);
        hasPendingSnapshot_ = false;

        watcher_.setFuture(QtConcurrent::run([store, snapshot = std::move(snapshot)] {
            store->save(snapshot);
            return true;
        }));
    }

    void UserPreferencesCoordinator::finishSave() {
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
