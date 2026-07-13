#pragma once

#include "infra/import/CancelableFileCopy.h"

#include <filesystem>
#include <stop_token>
#include <string>

namespace ssa::infra::importing {

    enum class LegacySpreadsheetConversionStatus {
        Succeeded,
        Canceled,
        ToolUnavailable,
        Failed,
    };

    struct LegacySpreadsheetConversionResult {
        LegacySpreadsheetConversionStatus status = LegacySpreadsheetConversionStatus::Failed;
        std::filesystem::path outputPath;
        std::string message;
        std::string diagnostic;

        [[nodiscard]] bool ok() const {
            return status == LegacySpreadsheetConversionStatus::Succeeded;
        }
    };

    class LegacySpreadsheetConverter final {
      public:
        LegacySpreadsheetConverter();
        explicit LegacySpreadsheetConverter(std::filesystem::path executablePath);

        [[nodiscard]] LegacySpreadsheetConversionResult
        convertToXlsx(const FileCopyRequest& request, const std::stop_token& stopToken = {}) const;

      private:
        [[nodiscard]] std::filesystem::path resolvedExecutable() const;

        std::filesystem::path executablePath_;
    };

} // namespace ssa::infra::importing
