#include "application/SsaWorkflowService.h"
#include "platform/ScrapReportSamRefreshPort.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QtConcurrentRun>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace {

    QString argumentValue(const QStringList& arguments, const QString& option) {
        const auto index = arguments.indexOf(option);
        return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString{};
    }

    int runFakeUv(const QStringList& arguments) {
        const auto sector = argumentValue(arguments, QStringLiteral("--executor-sector"));
        const auto project = argumentValue(arguments, QStringLiteral("--project"));
        const auto caFile = argumentValue(arguments, QStringLiteral("--ca-file"));
        const QStringList commandPrefix{QStringLiteral("run"),
                                        QStringLiteral("--project"),
                                        project,
                                        QStringLiteral("python"),
                                        QStringLiteral("-m"),
                                        QStringLiteral("scrap_report.cli"),
                                        QStringLiteral("sam-api-flow")};
        if (arguments.sliced(0, 7) != commandPrefix ||
            argumentValue(arguments, QStringLiteral("--profile")) != QStringLiteral("panorama") ||
            argumentValue(arguments, QStringLiteral("--base-url")) !=
                QStringLiteral("https://apps.itaipu.gov.br/SAM_SMA_API/rest/SSA_API") ||
            argumentValue(arguments, QStringLiteral("--number-of-years")) != QStringLiteral("4") ||
            argumentValue(arguments, QStringLiteral("--limit")) != QStringLiteral("200") ||
            argumentValue(arguments, QStringLiteral("--timeout-seconds")) != QStringLiteral("30") ||
            project.isEmpty() || caFile.isEmpty() ||
            !arguments.contains(QStringLiteral("--include-details"))) {
            return 2;
        }
        if (sector == QStringLiteral("BLOCK")) {
            QThread::msleep(5000);
        }
        const auto manifestPath = argumentValue(arguments, QStringLiteral("--output-json"));
        const auto xlsxPath = argumentValue(arguments, QStringLiteral("--output-xlsx"));
        if (manifestPath.isEmpty() || xlsxPath.isEmpty()) {
            return 3;
        }
        QDir{}.mkpath(QFileInfo{manifestPath}.absolutePath());
        if (sector != QStringLiteral("NOFILE")) {
            QFile xlsx{xlsxPath};
            if (!xlsx.open(QIODevice::WriteOnly) || xlsx.write("fake-xlsx") < 0) {
                return 4;
            }
        }
        QJsonObject exports;
        const auto exportPath = sector == QStringLiteral("EXPORT") ? manifestPath : xlsxPath;
        exports.insert(sector == QStringLiteral("ALIAS") ? QStringLiteral("xlsx")
                                                         : QStringLiteral("data_xlsx"),
                       exportPath);
        QJsonObject manifest;
        manifest.insert(QStringLiteral("status"), sector == QStringLiteral("BAD")
                                                      ? QStringLiteral("error")
                                                      : QStringLiteral("ok"));
        manifest.insert(QStringLiteral("runtime_mode"), sector == QStringLiteral("RUNTIME")
                                                            ? QStringLiteral("playwright")
                                                            : QStringLiteral("rest"));
        manifest.insert(QStringLiteral("profile"), sector == QStringLiteral("PROFILE")
                                                       ? QStringLiteral("detail-lote")
                                                       : QStringLiteral("panorama"));
        manifest.insert(QStringLiteral("verify_tls"), sector != QStringLiteral("TLS"));
        manifest.insert(QStringLiteral("count"), 1);
        QJsonObject filters;
        QJsonArray executorSectors;
        executorSectors.append(sector == QStringLiteral("MISMATCH") ? QStringLiteral("OTHER")
                                                                    : sector);
        filters.insert(QStringLiteral("executor_sectors"), executorSectors);
        manifest.insert(QStringLiteral("filters"), filters);
        if (sector == QStringLiteral("EMPTY")) {
            manifest.insert(QStringLiteral("count"), 0);
        }
        manifest.insert(QStringLiteral("exports"), exports);
        QFile output{manifestPath};
        if (!output.open(QIODevice::WriteOnly) ||
            output.write(QJsonDocument{manifest}.toJson(QJsonDocument::Compact)) < 0) {
            return 5;
        }
        std::fputs("ignored stdout from scrap_report\n", stdout);
        std::fflush(stdout);
        return 0;
    }

    void writeFile(const QString& path, const QByteArray& content = "fixture") {
        QDir{}.mkpath(QFileInfo{path}.absolutePath());
        QFile file{path};
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }

    ssa::ports::SamRefreshRequest validRequest(const QTemporaryDir& root) {
        const auto project = root.filePath(QStringLiteral("scrap_report"));
        writeFile(QDir{project}.filePath(QStringLiteral("pyproject.toml")));
        writeFile(QDir{project}.filePath(QStringLiteral("uv.lock")));
        writeFile(QDir{project}.filePath(QStringLiteral("src/scrap_report/cli.py")));
        const auto caFile = root.filePath(QStringLiteral("itaipu.pem"));
        writeFile(caFile, "certificate");
        ssa::ports::SamRefreshRequest request;
        request.enabled = true;
        request.scrapReportRoot = project.toStdString();
        request.caFile = caFile.toStdString();
        request.baseUrl = "https://apps.itaipu.gov.br/SAM_SMA_API/rest/SSA_API";
        request.executorSectors = {"IEE3", "MEL4"};
        request.scope = "consulta";
        request.intervalMinutes = 60;
        request.processTimeout = std::chrono::seconds{2};
        return request;
    }

    class CapturingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request,
                            std::stop_token = {}) override {
            ++calls;
            files = request.files;
            return nextResult;
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest&,
                                          std::stop_token = {}) override {
            return {ssa::ports::WorkflowStatus::NotImplemented, "not used"};
        }

        int calls = 0;
        std::vector<std::filesystem::path> files;
        ssa::ports::WorkflowResult nextResult{ssa::ports::WorkflowStatus::Succeeded,
                                              "import committed"};
    };

    class FakeSamPort final : public ssa::ports::ISamRefreshPort {
      public:
        ssa::ports::SamFetchResult fetch(const ssa::ports::SamRefreshRequest&,
                                         std::stop_token = {}) override {
            return nextResult;
        }

        bool discardArtifacts() override {
            ++discardCalls;
            return discardSucceeds;
        }

        ssa::ports::SamFetchResult nextResult;
        int discardCalls = 0;
        bool discardSucceeds = true;
    };

    class SamRefreshWorkflowTests final : public QObject {
        Q_OBJECT

      private slots:
        void port_fetches_one_fresh_rest_artifact_per_sector() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());

            const auto result = port.fetch(request);

            QVERIFY(result.ok());
            QCOMPARE(result.artifacts.size(), std::size_t{2});
            for (const auto& artifact : result.artifacts) {
                QVERIFY(std::filesystem::is_regular_file(artifact));
            }
            port.discardArtifacts();
            for (const auto& artifact : result.artifacts) {
                QVERIFY(!std::filesystem::exists(artifact));
            }
        }

        void port_rejects_invalid_preflight_without_starting_process() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.scope = "executadas";
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());

            const auto result = port.fetch(request);

            QCOMPARE(result.status, ssa::ports::WorkflowStatus::Rejected);
            QVERIFY(result.message.find("scope") != std::string::npos);
        }

        void port_rejects_entire_batch_when_one_manifest_fails() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"IEE3", "BAD"};
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());

            const auto result = port.fetch(request);

            QCOMPARE(result.status, ssa::ports::WorkflowStatus::Failed);
            QVERIFY(result.artifacts.empty());
        }

        void port_rejects_invalid_manifest_contract() {
            for (const auto& sector :
                 {std::string{"BAD"}, std::string{"RUNTIME"}, std::string{"PROFILE"},
                  std::string{"TLS"}, std::string{"EMPTY"}, std::string{"MISMATCH"},
                  std::string{"EXPORT"}, std::string{"ALIAS"}, std::string{"NOFILE"}}) {
                QTemporaryDir root;
                QVERIFY(root.isValid());
                auto request = validRequest(root);
                request.executorSectors = {sector};
                ssa::platform::ScrapReportSamRefreshPort port(
                    QCoreApplication::applicationFilePath().toStdString());

                const auto result = port.fetch(request);

                QCOMPARE(result.status, ssa::ports::WorkflowStatus::Failed);
                QVERIFY(result.artifacts.empty());
            }
        }

        void port_rejects_base_url_with_query_or_fragment() {
            for (const auto* suffix : {"?token=secret", "#fragment"}) {
                QTemporaryDir root;
                QVERIFY(root.isValid());
                auto request = validRequest(root);
                request.baseUrl += suffix;
                ssa::platform::ScrapReportSamRefreshPort port(
                    QCoreApplication::applicationFilePath().toStdString());

                const auto result = port.fetch(request);

                QCOMPARE(result.status, ssa::ports::WorkflowStatus::Rejected);
                QVERIFY(result.message.find("query") != std::string::npos);
            }
        }

        void port_honors_stop_token_and_kills_process() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"BLOCK"};
            request.processTimeout = std::chrono::seconds{10};
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());
            std::stop_source stopSource;

            auto future =
                QtConcurrent::run([&] { return port.fetch(request, stopSource.get_token()); });
            QTest::qWait(100);
            stopSource.request_stop();
            future.waitForFinished();

            QCOMPARE(future.result().status, ssa::ports::WorkflowStatus::Canceled);
        }

        void service_imports_only_complete_fetch_and_discards_artifacts() {
            auto importPort = std::make_shared<CapturingImportPort>();
            auto samPort = std::make_shared<FakeSamPort>();
            samPort->nextResult = {
                ssa::ports::WorkflowStatus::Succeeded, "fetched", {"first.xlsx", "second.xlsx"}};
            const ssa::application::SsaWorkflowService service(importPort, nullptr, nullptr,
                                                               nullptr, samPort);

            const auto result = service.refreshSam({});

            QVERIFY(result.ok());
            QCOMPARE(importPort->calls, 1);
            QCOMPARE(importPort->files.size(), std::size_t{2});
            QCOMPARE(samPort->discardCalls, 1);
        }

        void service_does_not_import_partial_or_failed_fetch() {
            auto importPort = std::make_shared<CapturingImportPort>();
            auto samPort = std::make_shared<FakeSamPort>();
            samPort->nextResult = {ssa::ports::WorkflowStatus::Failed, "sector failed", {}};
            const ssa::application::SsaWorkflowService service(importPort, nullptr, nullptr,
                                                               nullptr, samPort);

            const auto result = service.refreshSam({});

            QCOMPARE(result.status, ssa::ports::WorkflowStatus::Failed);
            QCOMPARE(importPort->calls, 0);
            QCOMPARE(samPort->discardCalls, 1);
        }

        void service_reports_cleanup_failure_after_committed_import() {
            auto importPort = std::make_shared<CapturingImportPort>();
            auto samPort = std::make_shared<FakeSamPort>();
            samPort->nextResult = {
                ssa::ports::WorkflowStatus::Succeeded, "fetched", {"fresh.xlsx"}};
            samPort->discardSucceeds = false;
            const ssa::application::SsaWorkflowService service(importPort, nullptr, nullptr,
                                                               nullptr, samPort);

            const auto result = service.refreshSam({});

            QVERIFY(result.ok());
            QVERIFY(result.warning);
            QVERIFY(result.message.find("could not be removed") != std::string::npos);
        }
    };

} // namespace

int main(int argc, char* argv[]) {
    const QStringList arguments = [&] {
        QStringList values;
        for (int index = 0; index < argc; ++index) {
            values.push_back(QString::fromLocal8Bit(argv[index]));
        }
        return values;
    }();
    if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("run")) {
        return runFakeUv(arguments.mid(1));
    }
    QCoreApplication application(argc, argv);
    SamRefreshWorkflowTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "SamRefreshWorkflowTests.moc"
