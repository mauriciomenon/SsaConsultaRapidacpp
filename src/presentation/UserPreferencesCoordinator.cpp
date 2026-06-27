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
        connect(&watcher_, &QFutureWatcher<QString>::finished, this,
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
            const auto error = watcher_.result();
            if (!error.isEmpty()) {
                qWarning() << "Failed to save preferences during shutdown:" << error;
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
        pendingSnapshotProvider_ = std::move(snapshotProvider);
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
        pendingSnapshotProvider_ = nullptr;
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
            try {
                store->save(snapshot);
                return QString{};
            } catch (const std::exception& exc) {
                return QString::fromUtf8(exc.what());
            } catch (...) {
                return QStringLiteral("erro desconhecido");
            }
        }));
    }

    void UserPreferencesCoordinator::finishSave() {
        if (!ensureOwnerThread("finishSave")) {
            return;
        }
        const auto error = watcher_.result();
        if (error.isEmpty()) {
            emit saved();
        } else {
            qWarning() << "Failed to save preferences:" << error;
            emit this->saveFailed(
                QStringLiteral("Falha interna ao salvar preferencias: %1").arg(error));
        }
        if (hasPendingSnapshot_ || pendingSnapshotProvider_) {
            flushPendingSave();
        }
    }

} // namespace ssa::presentation
