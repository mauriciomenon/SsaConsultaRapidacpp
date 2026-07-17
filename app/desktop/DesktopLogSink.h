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
        DesktopLogSink(const std::filesystem::path& configDirectory,
                       ssa::presentation::RecentLogModel& model);
        ~DesktopLogSink();

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
