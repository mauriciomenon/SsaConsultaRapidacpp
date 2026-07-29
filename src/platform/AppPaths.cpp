#include "platform/AppPaths.h"

#include "qt/FilesystemPath.h"

#include <QDir>

#include <filesystem>
#include <utility>

namespace ssa::platform {

    AppPaths::AppPaths(QString projectRoot, QString configDir)
        : projectRoot_(std::move(projectRoot)), configDir_(std::move(configDir)) {}

    QString AppPaths::preferencesFile() const {
        return QDir(configDir_).filePath("ssa_cpp_preferences.json");
    }

    std::filesystem::path AppPaths::projectRootPath() const {
        return qt::toFileSystemPath(projectRoot_);
    }

    std::filesystem::path AppPaths::configDirectoryPath() const {
        return qt::toFileSystemPath(configDir_);
    }

    std::filesystem::path AppPaths::inputFolderPath() const {
        return projectRootPath() / "docs_entrada";
    }

    std::filesystem::path AppPaths::processedFolderPath() const {
        return inputFolderPath() / "processadas";
    }

    std::filesystem::path AppPaths::redundantFolderPath() const {
        return processedFolderPath() / "nosurvivor";
    }

    std::filesystem::path AppPaths::installationGuidePath() const {
        return projectRootPath() / "README.md";
    }

    void AppPaths::ensureConfigDirectory() const {
        std::filesystem::create_directories(configDirectoryPath());
    }

    void AppPaths::ensureDataDirectory() const {
        std::filesystem::create_directories(projectRootPath() / "data");
    }

    void AppPaths::ensureInputFolders() const {
        std::filesystem::create_directories(redundantFolderPath());
    }

} // namespace ssa::platform
