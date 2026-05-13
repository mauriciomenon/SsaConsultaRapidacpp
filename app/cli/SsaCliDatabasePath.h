#pragma once

#include <QCommandLineParser>

#include <filesystem>

namespace ssa::app::cli {

    class SsaCliDatabasePath final {
      public:
        [[nodiscard]] static std::filesystem::path required(const QCommandLineParser& parser);
    };

} // namespace ssa::app::cli
