#include "platform/StartupOptions.h"

#include <QDir>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace ssa::platform {

    namespace {

        QString toQString(const std::filesystem::path& path) {
            return QString::fromStdString(path.string());
        }

        std::filesystem::path normalizedPath(const QString& value) {
            return std::filesystem::absolute(std::filesystem::path{value.toStdString()})
                .lexically_normal();
        }

        QString normalizedExistingDirectory(const QString& value, const char* label) {
            const auto path = normalizedPath(value);
            if (!std::filesystem::exists(path)) {
                throw std::invalid_argument(std::string{label} + " does not exist");
            }
            if (!std::filesystem::is_directory(path)) {
                throw std::invalid_argument(std::string{label} + " is not a directory");
            }
            return toQString(std::filesystem::weakly_canonical(path));
        }

        QString normalizedDatabasePath(const QString& value, const char* label) {
            const auto path = normalizedPath(value);
            if (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path)) {
                throw std::invalid_argument(std::string{label} + " is not a regular file");
            }
            const auto parent = path.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                throw std::invalid_argument(std::string{label} + " directory does not exist");
            }
            if (std::filesystem::exists(path)) {
                return toQString(std::filesystem::weakly_canonical(path));
            }
            return toQString(path);
        }

        QString normalizedWritableDirectoryPath(const QString& value, const char* label) {
            const auto path = normalizedPath(value);
            if (std::filesystem::exists(path) && !std::filesystem::is_directory(path)) {
                throw std::invalid_argument(std::string{label} + " is not a directory");
            }
            if (std::filesystem::exists(path)) {
                return toQString(std::filesystem::weakly_canonical(path));
            }
            return toQString(path);
        }

    } // namespace

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
        options.projectRoot = normalizedExistingDirectory(options.projectRoot, "project root");
        options.databasePath = normalizedDatabasePath(options.databasePath, "database path");
        options.configDir = normalizedWritableDirectoryPath(options.configDir, "config dir");
        return options;
    }

} // namespace ssa::platform
