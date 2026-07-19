#include "platform/ScrapReportSamRefreshPort.h"

#include "platform/SupervisedProcess.h"
#include "qt/FilesystemPath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace ssa::platform {
    namespace {

        using namespace std::chrono_literals;

        ports::SamFetchResult rejected(std::string message) {
            return {ports::WorkflowStatus::Rejected, std::move(message), {}};
        }

        ports::SamFetchResult canceled(std::string message, std::string diagnostic = {}) {
            return {ports::WorkflowStatus::Canceled, std::move(message), {}, std::move(diagnostic)};
        }

        ports::SamFetchResult failed(std::string message, std::string diagnostic = {}) {
            return {ports::WorkflowStatus::Failed, std::move(message), {}, std::move(diagnostic)};
        }

        bool isRegularFile(const std::filesystem::path& path) {
            std::error_code error;
            return std::filesystem::is_regular_file(path, error) && !error;
        }

        bool isDirectory(const std::filesystem::path& path) {
            std::error_code error;
            return std::filesystem::is_directory(path, error) && !error;
        }

        std::optional<std::string> validateRequest(const ports::SamRefreshRequest& request) {
            if (!request.enabled) {
                return "SAM refresh is disabled";
            }
            if (request.scope != "consulta") {
                return "SAM scope must be consulta";
            }
            if (request.intervalMinutes < 1 || request.intervalMinutes > 30'000) {
                return "SAM interval must be between 1 and 30000 minutes";
            }
            if (request.processTimeout <= 0ms) {
                return "SAM process timeout must be positive";
            }
            ports::ImportExecutionOptions execution;
            execution.rowsPerChunk = request.rowsPerChunk;
            execution.sqliteBusyWait = request.sqliteBusyWait;
            if (const auto validation = execution.validationError(); !validation.empty()) {
                return "SAM invalid_import_execution_options " + validation;
            }
            if (!isDirectory(request.scrapReportRoot) ||
                !isRegularFile(request.scrapReportRoot / "pyproject.toml") ||
                !isRegularFile(request.scrapReportRoot / "uv.lock") ||
                !isRegularFile(request.scrapReportRoot / "src/scrap_report/cli.py")) {
                return "scrap_report project is incomplete";
            }
            std::error_code caError;
            const auto caSize = std::filesystem::file_size(request.caFile, caError);
            if (!isRegularFile(request.caFile) || caError || caSize == 0) {
                return "CA file must be a non-empty regular file";
            }

            const QUrl baseUrl{QString::fromStdString(request.baseUrl)};
            if (!baseUrl.isValid() || baseUrl.scheme() != QStringLiteral("https") ||
                baseUrl.host().isEmpty() || !baseUrl.userInfo().isEmpty() || baseUrl.hasQuery() ||
                baseUrl.hasFragment()) {
                return "SAM base URL must be HTTPS without user information, query, or fragment";
            }
            if (request.executorSectors.empty()) {
                return "at least one executor sector is required";
            }
            static const QRegularExpression sectorPattern{QStringLiteral("^[A-Z0-9]{1,16}$")};
            std::unordered_set<std::string> uniqueSectors;
            for (const auto& sector : request.executorSectors) {
                if (!sectorPattern.match(QString::fromStdString(sector)).hasMatch()) {
                    return "executor sectors must use uppercase letters and digits";
                }
                if (!uniqueSectors.insert(sector).second) {
                    return "executor sectors must be unique";
                }
            }
            return std::nullopt;
        }

        struct ManifestValidationRequest {
            const QString& manifestPath;
            const QString& artifactPath;
            const QString& sector;
        };

        std::optional<std::size_t> jsonCount(const QJsonValue& value) {
            if (!value.isDouble()) {
                return std::nullopt;
            }
            const auto parsed = value.toInteger(-1);
            return parsed >= 0 ? std::optional<std::size_t>{static_cast<std::size_t>(parsed)}
                               : std::nullopt;
        }

        std::optional<ports::SamArtifact>
        validatedManifest(const ManifestValidationRequest& request) {
            constexpr qint64 kMaxManifestBytes = qint64{1024} * 1024;
            QFile file{request.manifestPath};
            if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 ||
                file.size() > kMaxManifestBytes) {
                return std::nullopt;
            }
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                return std::nullopt;
            }
            const auto root = document.object();
            const auto exports = root.value(QStringLiteral("exports")).toObject();
            const auto filters = root.value(QStringLiteral("filters")).toObject();
            const auto telemetry = root.value(QStringLiteral("telemetry")).toObject();
            const auto summary = root.value(QStringLiteral("summary")).toObject();
            const auto byExecutor = summary.value(QStringLiteral("by_executor")).toObject();
            const auto executorSectors =
                filters.value(QStringLiteral("executor_sectors")).toArray();
            const auto exportedPath = exports.value(QStringLiteral("data_xlsx")).toString();
            const QFileInfo artifactInfo{request.artifactPath};
            const auto count = jsonCount(root.value(QStringLiteral("count")));
            const auto telemetryRecords =
                jsonCount(telemetry.value(QStringLiteral("record_count")));
            const auto telemetryDetails =
                jsonCount(telemetry.value(QStringLiteral("detail_count")));
            const auto telemetryWithout =
                jsonCount(telemetry.value(QStringLiteral("without_detail_count")));
            const auto summaryTotal = jsonCount(summary.value(QStringLiteral("total")));
            const auto summaryDetails = jsonCount(summary.value(QStringLiteral("detail_count")));
            const auto summaryWithout =
                jsonCount(summary.value(QStringLiteral("without_detail_count")));
            const auto executorCount = jsonCount(byExecutor.value(request.sector));
            const bool valid =
                root.value(QStringLiteral("status")).toString() == QStringLiteral("ok") &&
                root.value(QStringLiteral("mode")).toString() == QStringLiteral("search") &&
                root.value(QStringLiteral("runtime_mode")).toString() == QStringLiteral("rest") &&
                root.value(QStringLiteral("profile")).toString() == QStringLiteral("panorama") &&
                root.value(QStringLiteral("verify_tls")).toBool(false) && count && *count > 0 &&
                filters.value(QStringLiteral("number_of_years")).toInt() == 4 &&
                filters.value(QStringLiteral("limit")).toInt() == 200 &&
                filters.value(QStringLiteral("include_details")).toBool(false) &&
                executorSectors.size() == 1 &&
                executorSectors.first().toString() == request.sector && telemetryRecords == count &&
                summaryTotal == count && telemetryDetails == summaryDetails &&
                telemetryWithout == summaryWithout && summaryDetails && summaryWithout &&
                *summaryDetails + *summaryWithout == *count && byExecutor.size() == 1 &&
                executorCount == count &&
                QFileInfo{exportedPath}.absoluteFilePath() ==
                    QFileInfo{request.artifactPath}.absoluteFilePath() &&
                !artifactInfo.isSymLink() && artifactInfo.isFile() && artifactInfo.size() > 0;
            if (!valid) {
                return std::nullopt;
            }
            return ports::SamArtifact{qt::toFileSystemPath(request.artifactPath),
                                      request.sector.toStdString(), *count, *summaryDetails,
                                      *summaryWithout};
        }

    } // namespace

    ScrapReportSamRefreshPort::ScrapReportSamRefreshPort(std::filesystem::path uvExecutable)
        : uvExecutable_(std::move(uvExecutable)) {}

    ScrapReportSamRefreshPort::~ScrapReportSamRefreshPort() = default;

    ports::SamFetchResult ScrapReportSamRefreshPort::fetch(const ports::SamRefreshRequest& request,
                                                           std::stop_token stopToken) {
        if (!discardArtifacts()) {
            return failed("could not remove previous SAM refresh artifacts");
        }
        if (stopToken.stop_requested()) {
            return canceled("SAM refresh was canceled");
        }
        if (const auto error = validateRequest(request)) {
            return rejected(*error);
        }

        const auto uvExecutable = uvExecutable_.empty()
                                      ? QStandardPaths::findExecutable(QStringLiteral("uv"))
                                      : qt::toQString(uvExecutable_);
        if (uvExecutable.isEmpty() || !QFileInfo{uvExecutable}.isExecutable()) {
            return rejected("uv executable was not found");
        }

        activeOutput_ = std::make_unique<QTemporaryDir>(QDir::tempPath() +
                                                        QStringLiteral("/ssa_sam_refresh_XXXXXX"));
        if (!activeOutput_->isValid()) {
            discardArtifacts();
            return failed("could not create the SAM refresh temporary directory");
        }

        std::vector<ports::SamArtifact> artifacts;
        artifacts.reserve(request.executorSectors.size());
        const auto deadline = std::chrono::steady_clock::now() + request.processTimeout;
        for (const auto& sector : request.executorSectors) {
            const auto outputDirectory =
                QDir{activeOutput_->path()}.filePath(QString::fromStdString(sector));
            if (!QDir{}.mkpath(outputDirectory)) {
                discardArtifacts();
                return failed("could not create a SAM sector output directory");
            }
            const auto manifestPath = QDir{outputDirectory}.filePath(QStringLiteral("result.json"));
            const auto artifactPath = QDir{outputDirectory}.filePath(QStringLiteral("result.xlsx"));

            const auto remainingBeforeStart = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remainingBeforeStart <= 0ms) {
                discardArtifacts();
                return failed("SAM refresh timed out");
            }
            const auto processResult =
                SupervisedProcess::run({.program = uvExecutable,
                                        .arguments = {QStringLiteral("run"),
                                                      QStringLiteral("--project"),
                                                      qt::toQString(request.scrapReportRoot),
                                                      QStringLiteral("python"),
                                                      QStringLiteral("-m"),
                                                      QStringLiteral("scrap_report.cli"),
                                                      QStringLiteral("sam-api-flow"),
                                                      QStringLiteral("--profile"),
                                                      QStringLiteral("panorama"),
                                                      QStringLiteral("--base-url"),
                                                      QString::fromStdString(request.baseUrl),
                                                      QStringLiteral("--executor-sector"),
                                                      QString::fromStdString(sector),
                                                      QStringLiteral("--number-of-years"),
                                                      QStringLiteral("4"),
                                                      QStringLiteral("--limit"),
                                                      QStringLiteral("200"),
                                                      QStringLiteral("--include-details"),
                                                      QStringLiteral("--timeout-seconds"),
                                                      QStringLiteral("30"),
                                                      QStringLiteral("--ca-file"),
                                                      qt::toQString(request.caFile),
                                                      QStringLiteral("--output-json"),
                                                      manifestPath,
                                                      QStringLiteral("--output-xlsx"),
                                                      artifactPath},
                                        .workingDirectory = qt::toQString(request.scrapReportRoot),
                                        .timeout = remainingBeforeStart},
                                       stopToken);
            if (processResult.status == SupervisedProcessStatus::Canceled) {
                discardArtifacts();
                return canceled("SAM refresh was canceled", processResult.diagnostic.toStdString());
            }
            if (processResult.status == SupervisedProcessStatus::TimedOut) {
                discardArtifacts();
                return failed("SAM refresh timed out", processResult.diagnostic.toStdString());
            }
            if (!processResult.ok()) {
                discardArtifacts();
                return failed("SAM refresh process failed", processResult.diagnostic.toStdString());
            }
            const auto sectorName = QString::fromStdString(sector);
            auto artifact = validatedManifest({manifestPath, artifactPath, sectorName});
            if (!artifact) {
                discardArtifacts();
                return failed("SAM refresh returned an invalid manifest");
            }
            if (artifact->manifestRows >= 200) {
                discardArtifacts();
                return rejected("SAM refresh result may be truncated at the configured limit");
            }
            artifacts.push_back(std::move(*artifact));
        }

        if (stopToken.stop_requested()) {
            discardArtifacts();
            return canceled("SAM refresh was canceled");
        }

        return {ports::WorkflowStatus::Succeeded, "SAM REST artifacts are ready",
                std::move(artifacts)};
    }

    bool ScrapReportSamRefreshPort::discardArtifacts() {
        if (!activeOutput_) {
            return true;
        }
        if (!activeOutput_->isValid()) {
            activeOutput_.reset();
            return true;
        }
        if (!QDir{activeOutput_->path()}.removeRecursively()) {
            return false;
        }
        activeOutput_.reset();
        return true;
    }

} // namespace ssa::platform
