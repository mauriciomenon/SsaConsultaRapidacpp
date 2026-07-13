#pragma once

#include <QString>
#include <QStringList>

#include <chrono>
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
                                                         std::stop_token stopToken = {});
    };

} // namespace ssa::platform
