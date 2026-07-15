#pragma once

#include <QString>
#include <QStringList>

#include <chrono>
#include <cstddef>
#include <stop_token>

namespace ssa::platform {

    enum class SupervisedProcessStatus {
        Succeeded,
        Canceled,
        TimedOut,
        StartFailed,
        Failed,
        FailedToStop,
    };

    enum class ForceStopRequestStatus {
        Drained,
        Pending,
        Failed,
    };

    struct SupervisedProcessRequest {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        std::chrono::milliseconds timeout{180'000};
    };

    struct SupervisedProcessResult {
        SupervisedProcessStatus status = SupervisedProcessStatus::Failed;
        int exitCode = -1;
        QString diagnostic;

        [[nodiscard]] bool ok() const {
            return status == SupervisedProcessStatus::Succeeded;
        }
    };

    class SupervisedProcess final {
      public:
        [[nodiscard]] static SupervisedProcessResult run(const SupervisedProcessRequest& request,
                                                         const std::stop_token& stopToken = {});
        [[nodiscard]] static ForceStopRequestStatus requestForceStopAll();
        [[nodiscard]] static bool forceStopAll();
    };

#ifdef SSA_SUPERVISED_PROCESS_TESTING
    namespace supervised_process_testing {
        void setStopFailure(bool enabled);
        void setPostStartPause(std::chrono::milliseconds duration);
        [[nodiscard]] std::size_t registeredTreeCount();
        void setUntrackedStopFailure(bool enabled);
        void recordTrackedStopFailure();
    } // namespace supervised_process_testing
#endif

} // namespace ssa::platform
