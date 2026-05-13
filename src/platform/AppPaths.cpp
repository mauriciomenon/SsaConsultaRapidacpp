#include "platform/AppPaths.h"

#include <QDir>

#include <filesystem>
#include <utility>

namespace ssa::platform {

    AppPaths::AppPaths(QString projectRoot, QString configDir)
        : projectRoot_(std::move(projectRoot)), configDir_(std::move(configDir)) {}

    QString AppPaths::projectRoot() const {
        return projectRoot_;
    }

    QString AppPaths::configDir() const {
        return configDir_;
    }

    QString AppPaths::preferencesFile() const {
        return QDir(configDir_).filePath("ssa_cpp_preferences.json");
    }

    std::filesystem::path AppPaths::projectRootPath() const {
        return std::filesystem::path{projectRoot_.toStdString()};
    }

    std::filesystem::path AppPaths::configDirectoryPath() const {
        return std::filesystem::path{configDir_.toStdString()};
    }

    std::filesystem::path AppPaths::inputFolderPath() const {
        return projectRootPath() / "docs_entrada";
    }

    std::filesystem::path AppPaths::processedFolderPath() const {
        return inputFolderPath() / "processadas";
    }

    std::filesystem::path AppPaths::redundantFolderPath() const {
        return inputFolderPath() / "nosurvivor";
    }

    std::filesystem::path AppPaths::installationGuidePath() const {
        return projectRootPath() / "README.md";
    }

    void AppPaths::ensureConfigDirectory() const {
        std::filesystem::create_directories(configDirectoryPath());
    }

} // namespace ssa::platform
