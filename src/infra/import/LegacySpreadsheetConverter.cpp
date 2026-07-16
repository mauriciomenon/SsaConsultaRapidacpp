#include "infra/import/LegacySpreadsheetConverter.h"

#include "infra/import/CancelableFileCopy.h"
#include "qt/FilesystemPath.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QtGlobal>

#include <chrono>
#include <memory>
#include <system_error>
#include <utility>

namespace ssa::infra::importing {

    namespace {

        std::filesystem::path executableFromEnvironment() {
            const auto value = qEnvironmentVariable("SSA_SOFFICE_PATH");
            if (!value.isEmpty()) {
                return qt::toFileSystemPath(value);
            }
            return {};
        }

        std::filesystem::path executableFromPath() {
            const QStringList names{
#ifdef _WIN32
                QStringLiteral("soffice.exe"),
                QStringLiteral("libreoffice.exe"),
#endif
                QStringLiteral("soffice"),
                QStringLiteral("libreoffice"),
            };
            for (const auto& name : names) {
                const auto found = QStandardPaths::findExecutable(name);
                if (!found.isEmpty()) {
                    return qt::toFileSystemPath(found);
                }
            }
            return {};
        }

        std::filesystem::path generatedXlsxPath(const std::filesystem::path& source,
                                                const std::filesystem::path& directory) {
            auto generated = directory / source.stem();
            generated.replace_extension(".xlsx");
            return generated;
        }

    } // namespace

    LegacySpreadsheetConverter::LegacySpreadsheetConverter(
        std::filesystem::path executablePath,
        std::shared_ptr<ports::IExternalProcessRunner> processRunner)
        : executablePath_(std::move(executablePath)), processRunner_(std::move(processRunner)) {}

    bool LegacySpreadsheetConverter::available() const {
        return processRunner_ != nullptr && !resolvedExecutable().empty();
    }

    LegacySpreadsheetConversionResult
    LegacySpreadsheetConverter::convertToXlsx(const FileCopyRequest& request,
                                              const std::stop_token& stopToken) const {
        const auto& source = request.source;
        const auto& destination = request.destination;
        if (stopToken.stop_requested()) {
            return {LegacySpreadsheetConversionStatus::Canceled, {}, "xls conversion canceled"};
        }
        const auto executable = resolvedExecutable();
        if (processRunner_ == nullptr || executable.empty()) {
            return {LegacySpreadsheetConversionStatus::ToolUnavailable,
                    {},
                    "xls converter unavailable",
                    processRunner_ == nullptr ? "external process runner is not configured"
                                              : "LibreOffice soffice executable was not found"};
        }

        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error) {
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "cannot create xls conversion directory",
                    error.message()};
        }
        QTemporaryDir conversionDirectory(
            qt::toQString(destination.parent_path() / "ssa_xls_conversion_XXXXXX"));
        if (!conversionDirectory.isValid()) {
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "cannot create xls conversion temporary directory"};
        }
        const auto conversionDirectoryPath =
            qt::toFileSystemPath(conversionDirectory.path()).lexically_normal();
        const auto cleanupConversionDirectory = [&conversionDirectory,
                                                 conversionDirectoryPath]() -> std::string {
            if (conversionDirectory.remove()) {
                return {};
            }
            std::error_code error;
            const bool remains = std::filesystem::exists(conversionDirectoryPath, error);
            std::string diagnostic = "cannot remove xls conversion temporary directory path=" +
                                     conversionDirectoryPath.string();
            if (error) {
                diagnostic += " error=" + error.message();
            } else if (remains) {
                diagnostic += " error=temporary directory remains";
            } else {
                diagnostic += " error=temporary directory already absent";
            }
            return diagnostic;
        };
        const auto processResult = processRunner_->run(
            {.program = executable,
             .arguments = {"--headless", "--convert-to", "xlsx", "--outdir",
                           qt::toUtf8(qt::toFileSystemPath(conversionDirectory.path())), "--",
                           qt::toUtf8(source)},
             .timeout = std::chrono::minutes{3}},
            stopToken);
        if (processResult.status == ports::ExternalProcessStatus::Canceled) {
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                return {LegacySpreadsheetConversionStatus::CleanupFailed,
                        {},
                        "cannot clean canceled xls conversion",
                        cleanupDiagnostic};
            }
            return {LegacySpreadsheetConversionStatus::Canceled, {}, "xls conversion canceled"};
        }
        if (!processResult.ok()) {
            auto diagnostic = processResult.diagnostic;
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                diagnostic += diagnostic.empty() ? cleanupDiagnostic : "; " + cleanupDiagnostic;
                return {LegacySpreadsheetConversionStatus::CleanupFailed,
                        {},
                        "cannot clean failed xls conversion",
                        std::move(diagnostic)};
            }
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "xls conversion failed",
                    std::move(diagnostic)};
        }

        const auto generated =
            generatedXlsxPath(source, qt::toFileSystemPath(conversionDirectory.path()));
        if (!std::filesystem::is_regular_file(generated, error) || error) {
            auto diagnostic = error.message();
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                diagnostic += diagnostic.empty() ? cleanupDiagnostic : "; " + cleanupDiagnostic;
                return {LegacySpreadsheetConversionStatus::CleanupFailed,
                        {},
                        "cannot clean invalid xls conversion",
                        std::move(diagnostic)};
            }
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "converted xls output was not created",
                    std::move(diagnostic)};
        }
        const auto copy = copyFileAtomically({generated, destination}, stopToken);
        if (copy.status == FileCopyStatus::Canceled) {
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                return {LegacySpreadsheetConversionStatus::CleanupFailed,
                        {},
                        "cannot clean canceled xls conversion",
                        cleanupDiagnostic};
            }
            return {LegacySpreadsheetConversionStatus::Canceled, {}, "xls conversion canceled"};
        }
        if (copy.status == FileCopyStatus::CleanupFailed) {
            auto diagnostic = copy.diagnostic;
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                diagnostic += diagnostic.empty() ? cleanupDiagnostic : "; " + cleanupDiagnostic;
            }
            return {LegacySpreadsheetConversionStatus::CleanupFailed,
                    {},
                    "cannot clean xls conversion output",
                    std::move(diagnostic)};
        }
        if (!copy.ok()) {
            auto diagnostic = copy.diagnostic;
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                diagnostic += diagnostic.empty() ? cleanupDiagnostic : "; " + cleanupDiagnostic;
                return {LegacySpreadsheetConversionStatus::CleanupFailed,
                        {},
                        "cannot clean failed xls conversion",
                        std::move(diagnostic)};
            }
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "cannot publish converted xls output",
                    std::move(diagnostic)};
        }
        const auto cleanupDiagnostic = cleanupConversionDirectory();
        if (!cleanupDiagnostic.empty()) {
            return {LegacySpreadsheetConversionStatus::Succeeded, destination,
                    "xls conversion succeeded with cleanup warning", cleanupDiagnostic};
        }
        return {LegacySpreadsheetConversionStatus::Succeeded, destination, {}};
    }

    std::filesystem::path LegacySpreadsheetConverter::resolvedExecutable() const {
        if (!executablePath_.empty()) {
            const QFileInfo executableInfo{qt::toQString(executablePath_)};
            return executableInfo.isFile() && executableInfo.isExecutable()
                       ? executablePath_
                       : std::filesystem::path{};
        }
        auto environmentPath = executableFromEnvironment();
        if (!environmentPath.empty()) {
            const QFileInfo executableInfo{qt::toQString(environmentPath)};
            return executableInfo.isFile() && executableInfo.isExecutable()
                       ? environmentPath
                       : std::filesystem::path{};
        }
        return executableFromPath();
    }

} // namespace ssa::infra::importing
