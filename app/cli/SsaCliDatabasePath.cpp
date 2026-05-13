#include "SsaCliDatabasePath.h"

#include <stdexcept>

namespace ssa::app::cli {

    std::filesystem::path SsaCliDatabasePath::required(const QCommandLineParser& parser) {
        if (!parser.isSet("db")) {
            throw std::invalid_argument("missing required --db");
        }

        const std::filesystem::path dbPath{parser.value("db").toStdString()};
        if (!std::filesystem::exists(dbPath)) {
            throw std::invalid_argument("database path does not exist");
        }
        if (!std::filesystem::is_regular_file(dbPath)) {
            throw std::invalid_argument("database path is not a regular file");
        }
        return dbPath;
    }

} // namespace ssa::app::cli
