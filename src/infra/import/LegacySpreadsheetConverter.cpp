#include "infra/import/LegacySpreadsheetConverter.h"

#include "qt/FilesystemPath.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include <chrono>
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
                                                const std::filesystem::path& destination) {
            auto generated = destination.parent_path() / source.stem();
            generated.replace_extension(".xlsx");
            return generated;
        }

        bool moveGeneratedFile(const std::filesystem::path& generated,
                               const std::filesystem::path& destination, std::string& message) {
            std::error_code error;
            if (!std::filesystem::exists(generated)) {
                message = "converted xls output was not created";
                return false;
            }
            if (generated == destination) {
                return true;
            }
            std::filesystem::remove(destination, error);
            error.clear();
            std::filesystem::rename(generated, destination, error);
            if (!error) {
                return true;
            }
            error.clear();
            std::filesystem::copy_file(generated, destination,
                                       std::filesystem::copy_options::overwrite_existing, error);
            if (error) {
                message = "cannot move converted xls output: " + error.message();
                return false;
            }
            std::filesystem::remove(generated, error);
            return true;
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
            return {LegacySpreadsheetConversionStatus::Failed, {}, "xls conversion canceled"};
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
                    "cannot create xls conversion directory: " + error.message()};
        }
        std::filesystem::remove(destination, error);

        QProcess process;
        process.setProgram(qt::toQString(executable));
        process.setArguments(QStringList{
            QStringLiteral("--headless"), QStringLiteral("--convert-to"), QStringLiteral("xlsx"),
            QStringLiteral("--outdir"), qt::toQString(destination.parent_path()),
            QStringLiteral("--"), qt::toQString(source)});
        process.start();
        QDeadlineTimer conversionDeadline(std::chrono::minutes{3});
        while (process.state() == QProcess::Starting && !conversionDeadline.hasExpired() &&
               !stopToken.stop_requested()) {
            process.waitForStarted(100);
        }
        while (process.state() == QProcess::Running && !conversionDeadline.hasExpired() &&
               !stopToken.stop_requested()) {
            process.waitForFinished(100);
        }
        if (stopToken.stop_requested()) {
            process.kill();
            if (!process.waitForFinished(5000)) {
                return {LegacySpreadsheetConversionStatus::Failed,
                        {},
                        "xls conversion canceled but process did not terminate"};
            }
            return {LegacySpreadsheetConversionStatus::Failed, {}, "xls conversion canceled"};
        }
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            if (!process.waitForFinished(30000)) {
                return {LegacySpreadsheetConversionStatus::Failed,
                        {},
                        "xls conversion timed out and did not terminate"};
            }
            return {LegacySpreadsheetConversionStatus::Failed, {}, "xls conversion timed out"};
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            const auto errorText = process.readAllStandardError();
            return {LegacySpreadsheetConversionStatus::Failed,
                    {},
                    "xls conversion failed: " + errorText.toStdString()};
        }

        std::string moveMessage;
        const auto generated = generatedXlsxPath(source, destination);
        if (stopToken.stop_requested()) {
            std::filesystem::remove(generated, error);
            return {LegacySpreadsheetConversionStatus::Failed, {}, "xls conversion canceled"};
        }
        if (!moveGeneratedFile(generated, destination, moveMessage)) {
            return {LegacySpreadsheetConversionStatus::Failed, {}, moveMessage};
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
