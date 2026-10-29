#pragma once

#include <QtLogging>

#include <filesystem>
#include <memory>
#include <mutex>

namespace ssa::presentation {
    class RecentLogModel;
}

namespace ssa::app::desktop {

    class DesktopLogSink final {
      public:
        explicit DesktopLogSink(const std::filesystem::path& configDirectory);
        ~DesktopLogSink();

        // The activity journal file written under the configuration directory.
        [[nodiscard]] static std::filesystem::path
        logFilePath(const std::filesystem::path& configDirectory) {
            return configDirectory / "logs" / "ssa.log";
        }

        void attachModel(ssa::presentation::RecentLogModel& model);
        // Must run before the attached model is destroyed.
        void detachModel();

        DesktopLogSink(const DesktopLogSink&) = delete;
        DesktopLogSink& operator=(const DesktopLogSink&) = delete;

      private:
        class HandlerState;

        static void messageHandler(QtMsgType type, const QMessageLogContext& context,
                                   const QString& message);

        static std::mutex handlerMutex_;
        static std::shared_ptr<HandlerState> active_;

        std::shared_ptr<HandlerState> state_;
    };

} // namespace ssa::app::desktop
