#pragma once

#include "ports/IUserPreferencesStore.h"

#include <QFutureWatcher>
#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>

namespace ssa::presentation {

    class UserPreferencesCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit UserPreferencesCoordinator(
            std::shared_ptr<ports::IUserPreferencesStore> preferencesStore,
            QObject* parent = nullptr);
        ~UserPreferencesCoordinator() override;

        [[nodiscard]] ports::UserPreferencesSnapshot loadInitial() const;
        void scheduleSave(std::function<ports::UserPreferencesSnapshot()> snapshotProvider);
        void saveNowOrSchedule(ports::UserPreferencesSnapshot snapshot);

      signals:
        void saved();
        void saveFailed(QString message);

      private slots:
        void flushPendingSave();
        void finishSave();

      private:
        bool ensureOwnerThread(const char* operation);

        // Null store is a supported no-persistence mode for tests and temporary sessions.
        std::shared_ptr<ports::IUserPreferencesStore> preferencesStore_;
        QFutureWatcher<bool> watcher_;
        QTimer saveTimer_;
        std::function<ports::UserPreferencesSnapshot()> pendingSnapshotProvider_;
        ports::UserPreferencesSnapshot pendingSnapshot_;
        bool hasPendingSnapshot_{false};
    };

} // namespace ssa::presentation
