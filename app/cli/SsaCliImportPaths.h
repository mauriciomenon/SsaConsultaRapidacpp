#pragma once

#include <QCommandLineParser>

#include <filesystem>

namespace ssa::app::cli {

    class SsaCliImportPaths final {
      public:
        [[nodiscard]] static std::filesystem::path
        docsDirectory(const QCommandLineParser& parser,
                      const std::filesystem::path& databaseFilePath);
        [[nodiscard]] static bool isRescanOperation(const QCommandLineParser& parser);
    };

} // namespace ssa::app::cli
