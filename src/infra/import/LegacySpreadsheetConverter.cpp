#include "infra/import/LegacySpreadsheetConverter.h"

#include "infra/import/CancelableFileCopy.h"
#include "platform/SupervisedProcess.h"
#include "qt/FilesystemPath.h"

#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QtGlobal>

#include <chrono>
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

    LegacySpreadsheetConverter::LegacySpreadsheetConverter() = default;

    LegacySpreadsheetConverter::LegacySpreadsheetConverter(std::filesystem::path executablePath)
        : executablePath_(std::move(executablePath)) {}

    LegacySpreadsheetConversionResult
    LegacySpreadsheetConverter::convertToXlsx(const std::filesystem::path& source,
                                              const std::filesystem::path& destination,
                                              const std::stop_token stopToken) const {
        if (stopToken.stop_requested()) {
            return {LegacySpreadsheetConversionStatus::Canceled, {}, "xls conversion canceled"};
        }
        const auto executable = resolvedExecutable();
        if (executable.empty()) {
            return {LegacySpreadsheetConversionStatus::ToolUnavailable,
                    {},
                    "LibreOffice soffice executable was not found"};
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
        const auto cleanupConversionDirectory = [&conversionDirectory]() -> std::string {
            return conversionDirectory.remove()
                       ? std::string{}
                       : std::string{"cannot remove xls conversion temporary directory"};
        };
        const auto processResult = platform::SupervisedProcess::run(
            {.program = qt::toQString(executable),
             .arguments = {QStringLiteral("--headless"), QStringLiteral("--convert-to"),
                           QStringLiteral("xlsx"), QStringLiteral("--outdir"),
                           conversionDirectory.path(), QStringLiteral("--"), qt::toQString(source)},
             .timeout = std::chrono::minutes{3}},
            stopToken);
        if (processResult.status == platform::SupervisedProcessStatus::Canceled) {
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                return {LegacySpreadsheetConversionStatus::Failed,
                        {},
                        "cannot clean canceled xls conversion",
                        cleanupDiagnostic};
            }
            return {LegacySpreadsheetConversionStatus::Canceled, {}, "xls conversion canceled"};
        }
        if (!processResult.ok()) {
            auto diagnostic = processResult.diagnostic.toStdString();
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                diagnostic += diagnostic.empty() ? cleanupDiagnostic : "; " + cleanupDiagnostic;
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
                return {LegacySpreadsheetConversionStatus::Failed,
                        {},
                        "cannot clean canceled xls conversion",
                        cleanupDiagnostic};
            }
            return {LegacySpreadsheetConversionStatus::Canceled, {}, "xls conversion canceled"};
        }
        if (!copy.ok()) {
            auto diagnostic = copy.diagnostic;
            const auto cleanupDiagnostic = cleanupConversionDirectory();
            if (!cleanupDiagnostic.empty()) {
                diagnostic += diagnostic.empty() ? cleanupDiagnostic : "; " + cleanupDiagnostic;
            }
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "cannot publish converted xls output",
                    std::move(diagnostic)};
        }
        const auto cleanupDiagnostic = cleanupConversionDirectory();
        if (!cleanupDiagnostic.empty()) {
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "cannot clean xls conversion temporary directory",
                    cleanupDiagnostic};
        }
        return {LegacySpreadsheetConversionStatus::Succeeded, destination, {}};
    }

    std::filesystem::path LegacySpreadsheetConverter::resolvedExecutable() const {
        if (!executablePath_.empty()) {
            return executablePath_;
        }
        auto environmentPath = executableFromEnvironment();
        if (!environmentPath.empty()) {
            return environmentPath;
        }
        return executableFromPath();
    }

} // namespace ssa::infra::importing
