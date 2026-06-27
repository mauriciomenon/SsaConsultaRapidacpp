#include "platform/LocalOpenCommandHandler.h"

#include <QDesktopServices>
#include <QUrl>

#include <map>

namespace ssa::platform {

    LocalOpenCommandHandler::LocalOpenCommandHandler(LocalOpenPaths paths, OpenPathPolicy policy)
        : paths_(std::move(paths)), policy_(std::move(policy)) {}

    ports::ExternalCommandResult LocalOpenCommandHandler::openPath(const std::string& path) const {
        auto validation = policy_.validate(path);
        if (!validation.ok()) {
            return validation;
        }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)))) {
            return {ports::ExternalCommandStatus::Failed, "failed to open path"};
        }
        return {ports::ExternalCommandStatus::Succeeded, "path opened"};
    }

    std::optional<ports::ExternalCommandResult>
    LocalOpenCommandHandler::executeConfigured(const ports::ExternalCommandKind kind) const {
        const auto path = configuredPathFor(kind);
        if (!path) {
            return std::nullopt;
        }
        return openConfiguredPath(*path);
    }

    ports::ExternalCommandResult
    LocalOpenCommandHandler::openConfiguredPath(const std::filesystem::path& path) const {
        if (path.empty()) {
            return {ports::ExternalCommandStatus::Rejected, "configured path is empty"};
        }
        return openPath(path.string());
    }

    std::optional<std::filesystem::path>
    LocalOpenCommandHandler::configuredPathFor(const ports::ExternalCommandKind kind) const {
        using PathMember = std::filesystem::path LocalOpenPaths::*;
        static const std::map<ports::ExternalCommandKind, PathMember> kConfiguredPaths{
            {ports::ExternalCommandKind::OpenInputFolder, &LocalOpenPaths::inputFolder},
            {ports::ExternalCommandKind::OpenProcessedFolder, &LocalOpenPaths::processedFolder},
            {ports::ExternalCommandKind::OpenRedundantFolder, &LocalOpenPaths::redundantFolder},
            {ports::ExternalCommandKind::OpenInstallationGuide, &LocalOpenPaths::installationGuide},
        };
        const auto it = kConfiguredPaths.find(kind);
        if (it == kConfiguredPaths.end()) {
            return std::nullopt;
        }
        return paths_.*(it->second);
    }

} // namespace ssa::platform
