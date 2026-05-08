#include "platform/AppPaths.h"

#include <QDir>

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

} // namespace ssa::platform
