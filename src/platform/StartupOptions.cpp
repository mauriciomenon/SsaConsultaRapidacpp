#include "platform/StartupOptions.h"

#include "qt/FilesystemPath.h"

#include <QCoreApplication>
#include <QDir>
#include <QUrl>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssa::platform {

    namespace {

        std::filesystem::path normalizedPath(const QString& value) {
            return std::filesystem::absolute(qt::toFileSystemPath(value)).lexically_normal();
        }

        void appendCandidateWithParents(std::vector<std::filesystem::path>& candidates,
                                        std::filesystem::path path) {
            path = std::filesystem::absolute(path).lexically_normal();
            while (!path.empty()) {
                candidates.push_back(path);
                const auto parent = path.parent_path();
                if (parent == path) {
                    break;
                }
                path = parent;
            }
        }

        bool containsDefaultDatabase(const std::filesystem::path& root) {
            const auto database = root / "data" / "ssas.db";
            std::error_code error;
            return std::filesystem::is_regular_file(database, error);
        }

        std::optional<std::filesystem::path> findProjectRootWithDatabase() {
            std::vector<std::filesystem::path> candidates;
            appendCandidateWithParents(candidates, qt::toFileSystemPath(QDir::currentPath()));
            if (QCoreApplication::instance() != nullptr) {
                const QString applicationDir = QCoreApplication::applicationDirPath();
                if (!applicationDir.isEmpty()) {
                    appendCandidateWithParents(candidates, qt::toFileSystemPath(applicationDir));
                }
            }
            for (const auto& candidate : candidates) {
                if (containsDefaultDatabase(candidate)) {
                    return candidate;
                }
            }
            return std::nullopt;
        }

        QString defaultProjectRoot() {
            if (const auto discovered = findProjectRootWithDatabase()) {
                return qt::toQString(std::filesystem::weakly_canonical(*discovered));
            }
            return QDir::currentPath();
        }

        QString normalizedExistingDirectory(const QString& value, const char* label) {
            const auto path = normalizedPath(value);
            if (!std::filesystem::exists(path)) {
                throw std::invalid_argument(std::string{label} + " does not exist");
            }
            if (!std::filesystem::is_directory(path)) {
                throw std::invalid_argument(std::string{label} + " is not a directory");
            }
            return qt::toQString(std::filesystem::weakly_canonical(path));
        }

        QString normalizedDatabasePath(const QString& value, const char* label,
                                       const bool requireParentDirectory) {
            const auto path = normalizedPath(value);
            if (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path)) {
                throw std::invalid_argument(std::string{label} + " is not a regular file");
            }
            const auto parent = path.parent_path();
            if (requireParentDirectory && !parent.empty() && !std::filesystem::exists(parent)) {
                throw std::invalid_argument(std::string{label} + " directory does not exist");
            }
            if (std::filesystem::exists(path)) {
                return qt::toQString(std::filesystem::weakly_canonical(path));
            }
            return qt::toQString(path);
        }

        QString normalizedOptionalDirectoryPath(const QString& value, const char* label) {
            const auto path = normalizedPath(value);
            if (std::filesystem::exists(path) && !std::filesystem::is_directory(path)) {
                throw std::invalid_argument(std::string{label} + " is not a directory");
            }
            if (std::filesystem::exists(path)) {
                return qt::toQString(std::filesystem::weakly_canonical(path));
            }
            return qt::toQString(path);
        }

        QString normalizedOptionalUrl(const QString& value, const char* label) {
            if (value.isEmpty()) {
                return {};
            }
            const QUrl url{value};
            if (!url.isValid() || url.scheme().isEmpty()) {
                throw std::invalid_argument(std::string{label} + " is not a valid absolute URL");
            }
            if (url.scheme() != "http" && url.scheme() != "https") {
                throw std::invalid_argument(std::string{label} + " must use http or https");
            }
            return value;
        }

    } // namespace

    QString StartupOptions::defaultSamBaseUrl() {
        return QStringLiteral("https://osprd.itaipu/SAM_SMA/");
    }

    StartupOptions StartupOptions::fromParser(const QCommandLineParser& parser) {
        StartupOptions options;
        options.projectRoot = parser.value("project-root");
        options.databasePath = parser.value("db");
        options.configDir = parser.value("config-dir");
        options.samBaseUrl = parser.value("sam-url");
        const bool explicitDatabasePath = parser.isSet("db");

        if (options.projectRoot.isEmpty()) {
            options.projectRoot = defaultProjectRoot();
        }
        if (options.databasePath.isEmpty()) {
            options.databasePath = QDir(options.projectRoot).filePath("data/ssas.db");
        }
        if (options.configDir.isEmpty()) {
            options.configDir = QDir(options.projectRoot).filePath("config");
        }
        if (options.samBaseUrl.isEmpty()) {
            options.samBaseUrl = defaultSamBaseUrl();
        }
        options.projectRoot = normalizedExistingDirectory(options.projectRoot, "project root");
        options.databasePath =
            normalizedDatabasePath(options.databasePath, "database path", explicitDatabasePath);
        options.configDir = normalizedOptionalDirectoryPath(options.configDir, "config dir");
        options.samBaseUrl = normalizedOptionalUrl(options.samBaseUrl, "SAM URL");
        return options;
    }

} // namespace ssa::platform
