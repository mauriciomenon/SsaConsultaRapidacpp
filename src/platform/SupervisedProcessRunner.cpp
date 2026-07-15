#include "platform/SupervisedProcessRunner.h"

#include "platform/SupervisedProcess.h"
#include "qt/FilesystemPath.h"

#include <QStringList>
#include <algorithm>

#include <utility>

namespace ssa::platform {

    ports::ExternalProcessResult
    SupervisedProcessRunner::run(const ports::ExternalProcessRequest& request,
                                 const std::stop_token& stopToken) const {
        QStringList arguments;
        arguments.reserve(static_cast<qsizetype>(request.arguments.size()));
        std::transform(request.arguments.cbegin(), request.arguments.cend(),
                       std::back_inserter(arguments), [](const std::string& argument) {
                           return QString::fromUtf8(argument.data(),
                                                    static_cast<qsizetype>(argument.size()));
                       });
        const auto result =
            SupervisedProcess::run({.program = qt::toQString(request.program),
                                    .arguments = std::move(arguments),
                                    .workingDirectory = qt::toQString(request.workingDirectory),
                                    .timeout = request.timeout},
                                   stopToken);

        ports::ExternalProcessResult output;
        output.exitCode = result.exitCode;
        output.diagnostic = result.diagnostic.toStdString();
        switch (result.status) {
        case SupervisedProcessStatus::Succeeded:
            output.status = ports::ExternalProcessStatus::Succeeded;
            break;
        case SupervisedProcessStatus::Canceled:
            output.status = ports::ExternalProcessStatus::Canceled;
            break;
        case SupervisedProcessStatus::TimedOut:
            output.status = ports::ExternalProcessStatus::TimedOut;
            break;
        case SupervisedProcessStatus::StartFailed:
            output.status = ports::ExternalProcessStatus::StartFailed;
            break;
        case SupervisedProcessStatus::FailedToStop:
            output.status = ports::ExternalProcessStatus::FailedToStop;
            break;
        case SupervisedProcessStatus::Failed:
            output.status = ports::ExternalProcessStatus::Failed;
            break;
        }
        return output;
    }

} // namespace ssa::platform
