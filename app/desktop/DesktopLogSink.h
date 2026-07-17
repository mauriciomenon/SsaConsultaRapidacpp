#pragma once

#include "platform/RotatingLogWriter.h"

#include <QtLogging>

#include <filesystem>
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
        static void messageHandler(QtMsgType type, const QMessageLogContext& context,
                                   const QString& message);
        void record(QtMsgType type, const QMessageLogContext& context, const QString& message);

        static std::mutex handlerMutex_;
        static DesktopLogSink* active_;

        ssa::platform::RotatingLogWriter writer_;
        ssa::presentation::RecentLogModel& model_;
        QtMessageHandler previousHandler_{nullptr};
    };

} // namespace ssa::app::desktop
