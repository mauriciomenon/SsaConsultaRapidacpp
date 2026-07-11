#include "platform/DesktopExternalCommandPort.h"

#include <QCoreApplication>
#include <QThread>

#include <map>

namespace ssa::platform {

    DesktopExternalCommandPort::DesktopExternalCommandPort(
        QUrl samBaseUrl, LocalOpenPaths paths, std::vector<std::filesystem::path> allowedOpenRoots)
        : sam_(std::move(samBaseUrl)),
          localOpen_(std::move(paths), OpenPathPolicy{std::move(allowedOpenRoots)}) {}

    ports::ExternalCommandResult
    DesktopExternalCommandPort::execute(const ports::ExternalCommand& command) {
        if (QCoreApplication::instance() == nullptr ||
            QCoreApplication::instance()->thread() != QThread::currentThread()) {
            return {ports::ExternalCommandStatus::Rejected,
                    "external commands must run on the GUI thread"};
        }
        using Handler = ports::ExternalCommandResult (DesktopExternalCommandPort::*)(
            const ports::ExternalCommand&);
        static const std::map<ports::ExternalCommandKind, Handler> kHandlers{
            {ports::ExternalCommandKind::OpenSamHome,
             &DesktopExternalCommandPort::handleOpenSamHome},
            {ports::ExternalCommandKind::OpenSsa, &DesktopExternalCommandPort::handleOpenSsa},
            {ports::ExternalCommandKind::OpenPath, &DesktopExternalCommandPort::handleOpenPath},
        };
        if (const auto it = kHandlers.find(command.kind); it != kHandlers.end()) {
            return (this->*(it->second))(command);
        }
        if (auto result = localOpen_.executeConfigured(command.kind)) {
            return *result;
        }
        return {ports::ExternalCommandStatus::NotImplemented,
                "external command is not implemented"};
    }

    ports::ExternalCommandResult
    DesktopExternalCommandPort::handleOpenSamHome(const ports::ExternalCommand& command) {
        Q_UNUSED(command);
        return sam_.openHome();
    }

    ports::ExternalCommandResult
    DesktopExternalCommandPort::handleOpenSsa(const ports::ExternalCommand& command) {
        const auto it = command.parameters.find("ssa_number");
        if (it == command.parameters.end() || it->second.empty()) {
            return {ports::ExternalCommandStatus::Rejected, "ssa_number is required"};
        }
        return sam_.openSsa(it->second);
    }

    ports::ExternalCommandResult
    DesktopExternalCommandPort::handleOpenPath(const ports::ExternalCommand& command) {
        const auto it = command.parameters.find("path");
        if (it == command.parameters.end() || it->second.empty()) {
            return {ports::ExternalCommandStatus::Rejected, "path is required"};
        }
        return localOpen_.openPath(it->second);
    }

} // namespace ssa::platform
