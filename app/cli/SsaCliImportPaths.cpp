#include "SsaCliImportPaths.h"

#include <QString>

namespace ssa::app::cli {

    std::filesystem::path
    SsaCliImportPaths::docsDirectory(const QCommandLineParser& parser,
                                     const std::filesystem::path& databaseFilePath) {
        if (parser.isSet("docs-dir")) {
            return std::filesystem::path{parser.value("docs-dir").toStdString()};
        }
        const auto absoluteDatabasePath =
            std::filesystem::absolute(databaseFilePath).lexically_normal();
        const auto databaseDirectory = absoluteDatabasePath.parent_path();
        if (databaseDirectory.filename() == "data") {
            return databaseDirectory.parent_path() / "docs_entrada";
        }
        const auto currentDocs = std::filesystem::current_path() / "docs_entrada";
        if (std::filesystem::exists(currentDocs)) {
            return currentDocs;
        }
        const auto databaseSiblingDocs = databaseDirectory / "docs_entrada";
        if (std::filesystem::exists(databaseSiblingDocs)) {
            return databaseSiblingDocs;
        }
        return {};
    }

    bool SsaCliImportPaths::isRescanOperation(const QCommandLineParser& parser) {
        return parser.isSet("rescan") || parser.isSet("force-rescan") ||
               parser.isSet("incremental-rescan");
    }

} // namespace ssa::app::cli
