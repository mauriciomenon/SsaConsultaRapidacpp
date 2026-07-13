#pragma once

#include "ports/IUserPreferencesStore.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>

namespace ssa::presentation {

    class UserPreferencesCoordinator final : public QObject {
        Q_OBJECT

      public:
        explicit UserPreferencesCoordinator(
            std::shared_ptr<ports::IUserPreferencesStore> preferencesStore,
            QObject* parent = nullptr);
        ~UserPreferencesCoordinator() override;

        [[nodiscard]] ports::UserPreferencesSnapshot loadInitial() const;
        [[nodiscard]] bool running() const;
        [[nodiscard]] bool canceling() const;
        [[nodiscard]] bool canCancel() const;
        void scheduleSave(std::function<ports::UserPreferencesSnapshot()> snapshotProvider);
        void saveNowOrSchedule(ports::UserPreferencesSnapshot snapshot);
        void beginShutdown(std::optional<ports::UserPreferencesSnapshot> finalSnapshot);
        void cancel();
        void shutdown();

      signals:
        void shutdownStarted();
        void shutdownFinished();
        void stateChanged();
        void saved();
        void saveFailed(QString message);

      private slots:
        void flushPendingSave();
        void finishSave();

      private:
        struct SaveTaskState final {
            std::mutex mutex;
            std::string error;
            bool canceled{false};
            bool finalSave{false};
        };

        bool ensureOwnerThread(const char* operation);
        void startSave(ports::UserPreferencesSnapshot snapshot, bool finalSave);
        void finishShutdown();

        // Null store is a supported no-persistence mode for tests and temporary sessions.
        std::shared_ptr<ports::IUserPreferencesStore> preferencesStore_;
        // void watcher: avoids QFutureInterface<QString>'s ResultStore, which has a
        // data race (QString move-emplace vs destruction) when the watcher is torn
        // down while the worker reports its result. The error is carried via a
        // mutex-protected std::string so the worker write and the finishSave read
        // are properly synchronized.
        QFutureWatcher<void> watcher_;
        QTimer saveTimer_;
        std::function<ports::UserPreferencesSnapshot()> snapshotProvider_;
        ports::UserPreferencesSnapshot pendingSnapshot_;
        std::shared_ptr<SaveTaskState> activeTask_;
        std::stop_source activeStopSource_;
        std::optional<ports::UserPreferencesSnapshot> finalSnapshot_;
        bool hasPendingSnapshot_{false};
        bool shuttingDown_{false};
        bool shutdownFinished_{false};
        bool running_{false};
        bool canceling_{false};
    };

} // namespace ssa::presentation
