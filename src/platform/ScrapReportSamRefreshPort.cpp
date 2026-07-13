#include "platform/ScrapReportSamRefreshPort.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
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

        ports::SamFetchResult canceled(std::string message) {
            return {ports::WorkflowStatus::Canceled, std::move(message), {}};
        }

        ports::SamFetchResult failed(std::string message) {
            return {ports::WorkflowStatus::Failed, std::move(message), {}};
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

        bool stopProcess(QProcess& process) {
            process.terminate();
            if (!process.waitForFinished(200)) {
                process.kill();
                if (!process.waitForFinished(1'000)) {
                    return false;
                }
            }
            return process.state() == QProcess::NotRunning;
        }

        std::string processFailureMessage(QProcess& process) {
            auto message = process.errorString().trimmed();
            const auto stderrText =
                QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            if (!stderrText.isEmpty()) {
                message = stderrText.left(512);
            }
            return message.toStdString();
        }

        bool validateManifest(const QString& manifestPath, const QString& artifactPath,
                              const QString& sector) {
            QFile file{manifestPath};
            if (!file.open(QIODevice::ReadOnly)) {
                return false;
            }
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                return false;
            }
            const auto root = document.object();
            const auto exports = root.value(QStringLiteral("exports")).toObject();
            const auto filters = root.value(QStringLiteral("filters")).toObject();
            const auto executorSectors =
                filters.value(QStringLiteral("executor_sectors")).toArray();
            const auto exportedPath = exports.value(QStringLiteral("data_xlsx")).toString();
            const QFileInfo artifactInfo{artifactPath};
            return root.value(QStringLiteral("status")).toString() == QStringLiteral("ok") &&
                   root.value(QStringLiteral("runtime_mode")).toString() ==
                       QStringLiteral("rest") &&
                   root.value(QStringLiteral("profile")).toString() == QStringLiteral("panorama") &&
                   root.value(QStringLiteral("verify_tls")).toBool(false) &&
                   root.value(QStringLiteral("count")).toInt(0) > 0 &&
                   executorSectors.size() == 1 && executorSectors.first().toString() == sector &&
                   QFileInfo{exportedPath}.absoluteFilePath() ==
                       QFileInfo{artifactPath}.absoluteFilePath() &&
                   !artifactInfo.isSymLink() && artifactInfo.isFile() && artifactInfo.size() > 0;
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
                                      : QString::fromStdString(uvExecutable_.string());
        if (uvExecutable.isEmpty() || !QFileInfo{uvExecutable}.isExecutable()) {
            return rejected("uv executable was not found");
        }

        activeOutput_ = std::make_unique<QTemporaryDir>(QDir::tempPath() +
                                                        QStringLiteral("/ssa_sam_refresh_XXXXXX"));
        if (!activeOutput_->isValid()) {
            discardArtifacts();
            return failed("could not create the SAM refresh temporary directory");
        }

        std::vector<std::filesystem::path> artifacts;
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

            QProcess process;
            process.setProgram(uvExecutable);
            process.setWorkingDirectory(QString::fromStdString(request.scrapReportRoot.string()));
            process.setArguments({QStringLiteral("run"),
                                  QStringLiteral("--project"),
                                  QString::fromStdString(request.scrapReportRoot.string()),
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
                                  QString::fromStdString(request.caFile.string()),
                                  QStringLiteral("--output-json"),
                                  manifestPath,
                                  QStringLiteral("--output-xlsx"),
                                  artifactPath});
            const auto remainingBeforeStart = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remainingBeforeStart <= 0ms) {
                discardArtifacts();
                return failed("SAM refresh timed out");
            }
            process.start();
            if (!process.waitForStarted(static_cast<int>(
                    std::min(remainingBeforeStart, std::chrono::milliseconds{5s}).count()))) {
                const auto message = processFailureMessage(process);
                discardArtifacts();
                return failed("could not start SAM refresh: " + message);
            }

            while (process.state() != QProcess::NotRunning) {
                process.readAllStandardOutput();
                if (stopToken.stop_requested()) {
                    const auto stopped = stopProcess(process);
                    discardArtifacts();
                    if (!stopped) {
                        return failed("could not stop the canceled SAM refresh process");
                    }
                    return canceled("SAM refresh was canceled");
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    const auto stopped = stopProcess(process);
                    discardArtifacts();
                    if (!stopped) {
                        return failed("could not stop the timed out SAM refresh process");
                    }
                    return failed("SAM refresh timed out");
                }
                process.waitForFinished(25);
            }
            process.readAllStandardOutput();
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                const auto message = processFailureMessage(process);
                discardArtifacts();
                return failed("SAM refresh process failed: " + message);
            }
            if (!validateManifest(manifestPath, artifactPath, QString::fromStdString(sector))) {
                discardArtifacts();
                return failed("SAM refresh returned an invalid manifest");
            }
            artifacts.emplace_back(artifactPath.toStdString());
        }

        return {ports::WorkflowStatus::Succeeded, "SAM REST artifacts are ready",
                std::move(artifacts)};
    }

    bool ScrapReportSamRefreshPort::discardArtifacts() {
        const auto removed = !activeOutput_ || activeOutput_->remove();
        activeOutput_.reset();
        return removed;
    }

} // namespace ssa::platform
