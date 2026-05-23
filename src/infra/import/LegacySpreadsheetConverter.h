#pragma once

#include <filesystem>
#include <string>

namespace ssa::infra::importing {

    enum class LegacySpreadsheetConversionStatus {
        Succeeded,
        ToolUnavailable,
        Failed,
    };

    struct LegacySpreadsheetConversionResult {
        LegacySpreadsheetConversionStatus status{LegacySpreadsheetConversionStatus::Failed};
        std::filesystem::path outputPath;
        std::string message;

        [[nodiscard]] bool ok() const {
            return status == LegacySpreadsheetConversionStatus::Succeeded;
        }
    };

    class LegacySpreadsheetConverter final {
      public:
        LegacySpreadsheetConverter();
        explicit LegacySpreadsheetConverter(std::filesystem::path executablePath);

        [[nodiscard]] LegacySpreadsheetConversionResult
        convertToXlsx(const std::filesystem::path& source,
                      const std::filesystem::path& destination) const;

      private:
        [[nodiscard]] std::filesystem::path resolvedExecutable() const;

        std::filesystem::path executablePath_;
    };

} // namespace ssa::infra::importing
