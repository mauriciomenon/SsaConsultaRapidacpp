#include "platform/StartupOptions.h"

#include <QDir>

namespace ssa::platform {

    StartupOptions StartupOptions::fromParser(const QCommandLineParser& parser) {
        StartupOptions options;
        options.projectRoot = parser.value("project-root");
        options.databasePath = parser.value("db");
        options.configDir = parser.value("config-dir");

        if (options.projectRoot.isEmpty()) {
            options.projectRoot = QDir::currentPath();
        }
        if (options.databasePath.isEmpty()) {
            options.databasePath = QDir(options.projectRoot).filePath("data/ssas.db");
        }
        if (options.configDir.isEmpty()) {
            options.configDir = QDir(options.projectRoot).filePath("config");
        }
        return options;
    }

} // namespace ssa::platform
