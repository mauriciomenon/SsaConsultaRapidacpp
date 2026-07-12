#include "SsaCliDatabasePath.h"

#include "qt/FilesystemPath.h"

#include <stdexcept>
#include <string>

namespace ssa::app::cli {

    std::filesystem::path SsaCliDatabasePath::required(const QCommandLineParser& parser) {
        if (!parser.isSet("db")) {
            throw std::invalid_argument("missing required --db");
        }

        const auto dbPath = ssa::qt::toFileSystemPath(parser.value("db"));
        if (!std::filesystem::exists(dbPath)) {
            throw std::invalid_argument("database path does not exist: " + ssa::qt::toUtf8(dbPath));
        }
        if (!std::filesystem::is_regular_file(dbPath)) {
            throw std::invalid_argument("database path is not a regular file: " +
                                        ssa::qt::toUtf8(dbPath));
        }
        return dbPath;
    }

} // namespace ssa::app::cli
