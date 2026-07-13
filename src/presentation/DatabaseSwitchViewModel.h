#pragma once

#include "ports/IDatabaseSwitchPorts.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>

#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>

namespace ssa::presentation {

    class DatabaseSwitchViewModel final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool running READ running NOTIFY runningChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

      public:
        DatabaseSwitchViewModel(std::shared_ptr<ports::IDatabaseValidator> validator,
                                std::shared_ptr<ports::IApplicationLauncher> launcher,
                                QObject* parent = nullptr);
        ~DatabaseSwitchViewModel() override;

        [[nodiscard]] bool running() const;
        [[nodiscard]] QString errorMessage() const;
        Q_INVOKABLE void openDatabase(const QUrl& url);
        void shutdown();

      signals:
        void runningChanged();
        void errorMessageChanged();
        void replacementStarted();

      private:
        struct ValidationState final {
            std::mutex mutex;
            std::optional<ports::DatabaseValidationResult> result;
            std::exception_ptr error;
        };

        void finishValidation();
        void setRunning(bool isRunning);
        void setErrorMessage(QString message);

        std::shared_ptr<ports::IDatabaseValidator> validator_;
        std::shared_ptr<ports::IApplicationLauncher> launcher_;
        QFutureWatcher<void> watcher_;
        std::shared_ptr<ValidationState> validationState_;
        std::stop_source validationStopSource_;
        std::filesystem::path pendingPath_;
        QString errorMessage_;
        bool running_ = false;
        bool shuttingDown_ = false;
    };

} // namespace ssa::presentation
