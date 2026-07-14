#include "application/SsaWorkflowService.h"
#include "platform/ScrapReportSamRefreshPort.h"
#include "platform/SupervisedProcess.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QtConcurrentRun>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace {

    QString argumentValue(const QStringList& arguments, const QString& option) {
        const auto index = arguments.indexOf(option);
        return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString{};
    }

    int runFakeUv(const QStringList& arguments, const QString& executablePath) {
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
        if (sector == QStringLiteral("NOISY")) {
            const QByteArray noise(qsizetype{2} * 1024 * 1024, 'x');
            std::fwrite(noise.constData(), 1, static_cast<std::size_t>(noise.size()), stdout);
            std::fwrite(noise.constData(), 1, static_cast<std::size_t>(noise.size()), stderr);
            std::fflush(stdout);
            std::fflush(stderr);
        } else if (sector == QStringLiteral("TREE")) {
            const auto sentinelPath = qEnvironmentVariable("SSA_TEST_SENTINEL_PATH");
            if (sentinelPath.isEmpty()) {
                return 6;
            }
            QProcess descendant;
            descendant.setProgram(executablePath);
            descendant.setArguments({QStringLiteral("--sentinel-child"), sentinelPath});
            descendant.start();
            if (!descendant.waitForStarted(2'000)) {
                return 7;
            }
            QThread::msleep(5'000);
        } else if (sector == QStringLiteral("BLOCK")) {
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

    int runSentinelChild(const QString& path) {
        QFile sentinel{path};
        if (!sentinel.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return 8;
        }
#ifdef Q_OS_WIN
        const auto processId = static_cast<qulonglong>(GetCurrentProcessId());
#else
        const auto processId = static_cast<qulonglong>(getpid());
#endif
        if (sentinel.write(QByteArray::number(processId)) < 0 || !sentinel.flush()) {
            return 9;
        }
        sentinel.close();
        QThread::msleep(30'000);
        return 0;
    }

    bool processExists(const qint64 processId) {
#ifdef Q_OS_WIN
        const auto process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(processId));
        if (process == nullptr) {
            return false;
        }
        const auto status = WaitForSingleObject(process, 0);
        CloseHandle(process);
        return status == WAIT_TIMEOUT;
#else
        return ::kill(static_cast<pid_t>(processId), 0) == 0 || errno == EPERM;
#endif
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

        void port_cancellation_terminates_the_entire_process_tree() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"TREE"};
            request.processTimeout = std::chrono::seconds{10};
            const auto sentinelPath = root.filePath(QStringLiteral("descendant.pid"));
            QVERIFY(qputenv("SSA_TEST_SENTINEL_PATH", sentinelPath.toUtf8()));
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());
            std::stop_source stopSource;

            auto future =
                QtConcurrent::run([&] { return port.fetch(request, stopSource.get_token()); });
            QElapsedTimer startupDeadline;
            startupDeadline.start();
            while (!QFileInfo::exists(sentinelPath) && startupDeadline.elapsed() < 3'000) {
                QTest::qWait(10);
            }
            QVERIFY2(QFileInfo::exists(sentinelPath), "descendant did not start");
            QFile sentinel{sentinelPath};
            QVERIFY(sentinel.open(QIODevice::ReadOnly));
            bool validPid = false;
            const auto descendantPid = sentinel.readAll().trimmed().toLongLong(&validPid);
            QVERIFY(validPid);
            QVERIFY(processExists(descendantPid));

            stopSource.request_stop();
            future.waitForFinished();
            qunsetenv("SSA_TEST_SENTINEL_PATH");

            QCOMPARE(future.result().status, ssa::ports::WorkflowStatus::Canceled);
            QElapsedTimer terminationDeadline;
            terminationDeadline.start();
            while (processExists(descendantPid) && terminationDeadline.elapsed() < 3'000) {
                QTest::qWait(10);
            }
            QVERIFY2(!processExists(descendantPid),
                     "descendant survived the cancellation terminal");
        }

        void forced_shutdown_terminates_every_registered_process_tree() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"TREE"};
            request.processTimeout = std::chrono::seconds{10};
            const auto sentinelPath = root.filePath(QStringLiteral("forced-descendant.pid"));
            QVERIFY(qputenv("SSA_TEST_SENTINEL_PATH", sentinelPath.toUtf8()));
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());

            auto future = QtConcurrent::run([&] { return port.fetch(request); });
            QElapsedTimer startupDeadline;
            startupDeadline.start();
            while (!QFileInfo::exists(sentinelPath) && startupDeadline.elapsed() < 3'000) {
                QTest::qWait(10);
            }
            QVERIFY2(QFileInfo::exists(sentinelPath), "descendant did not start");
            QFile sentinel{sentinelPath};
            QVERIFY(sentinel.open(QIODevice::ReadOnly));
            bool validPid = false;
            const auto descendantPid = sentinel.readAll().trimmed().toLongLong(&validPid);
            QVERIFY(validPid);
            QVERIFY(processExists(descendantPid));

            QElapsedTimer forceStopTimer;
            forceStopTimer.start();
            QCOMPARE(ssa::platform::SupervisedProcess::requestForceStopAll(),
                     ssa::platform::ForceStopRequestStatus::Ready);
            QVERIFY(forceStopTimer.elapsed() < 50);
            future.waitForFinished();
            QVERIFY(ssa::platform::SupervisedProcess::forceStopAll());
            qunsetenv("SSA_TEST_SENTINEL_PATH");

            QVERIFY2(!processExists(descendantPid),
                     "descendant survived the forced shutdown barrier");
        }

        void port_drains_noisy_process_channels_without_unbounded_diagnostics() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"NOISY"};
            request.processTimeout = std::chrono::seconds{5};
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());

            const auto result = port.fetch(request);

            QVERIFY(result.ok());
            QVERIFY(result.diagnostic.empty());
            QCOMPARE(result.artifacts.size(), std::size_t{1});
        }

        void port_does_not_start_when_token_is_already_stopped() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"TREE"};
            const auto sentinelPath = root.filePath(QStringLiteral("not-started.pid"));
            QVERIFY(qputenv("SSA_TEST_SENTINEL_PATH", sentinelPath.toUtf8()));
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());
            std::stop_source stopSource;
            stopSource.request_stop();

            const auto result = port.fetch(request, stopSource.get_token());
            qunsetenv("SSA_TEST_SENTINEL_PATH");

            QCOMPARE(result.status, ssa::ports::WorkflowStatus::Canceled);
            QVERIFY(!QFileInfo::exists(sentinelPath));
        }

#ifndef Q_OS_WIN
        void port_retains_failed_cleanup_for_a_verified_retry() {
            if (::geteuid() == 0) {
                QSKIP("permission cleanup failure cannot be simulated as root");
            }
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"IEE3"};
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());
            const auto fetch = port.fetch(request);
            QVERIFY(fetch.ok());
            QCOMPARE(fetch.artifacts.size(), std::size_t{1});
            const auto outputDirectory = fetch.artifacts.front().parent_path();
            std::filesystem::permissions(outputDirectory,
                                         std::filesystem::perms::owner_read |
                                             std::filesystem::perms::owner_exec,
                                         std::filesystem::perm_options::replace);

            QVERIFY(!port.discardArtifacts());
            QVERIFY(std::filesystem::exists(outputDirectory));

            std::filesystem::permissions(outputDirectory, std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::add);
            QVERIFY(port.discardArtifacts());
            QVERIFY(!std::filesystem::exists(outputDirectory));
        }
#endif

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
            QVERIFY(result.diagnostic.find("cleanup") != std::string::npos);
        }

        void service_preserves_failed_and_canceled_results_when_cleanup_fails() {
            for (const auto status :
                 {ssa::ports::WorkflowStatus::Failed, ssa::ports::WorkflowStatus::Canceled}) {
                auto importPort = std::make_shared<CapturingImportPort>();
                auto samPort = std::make_shared<FakeSamPort>();
                samPort->nextResult = {status, "primary result", {}};
                samPort->discardSucceeds = false;
                const ssa::application::SsaWorkflowService service(importPort, nullptr, nullptr,
                                                                   nullptr, samPort);

                const auto result = service.refreshSam({});

                QCOMPARE(result.status, status);
                QVERIFY(result.warning);
                QVERIFY(result.message.find("primary result") != std::string::npos);
                QVERIFY(result.diagnostic.find("cleanup") != std::string::npos);
                QCOMPARE(importPort->calls, 0);
            }
        }

        void forced_shutdown_prevents_a_concurrent_process_start() {
            QTemporaryDir root;
            QVERIFY(root.isValid());
            auto request = validRequest(root);
            request.executorSectors = {"TREE"};
            request.processTimeout = std::chrono::milliseconds{250};
            const auto sentinelPath = root.filePath(QStringLiteral("startup-race.pid"));
            QVERIFY(qputenv("SSA_TEST_SENTINEL_PATH", sentinelPath.toUtf8()));
            ssa::platform::ScrapReportSamRefreshPort port(
                QCoreApplication::applicationFilePath().toStdString());

            auto future = QtConcurrent::run([&] { return port.fetch(request); });
            QElapsedTimer forceStopTimer;
            forceStopTimer.start();
            QVERIFY(ssa::platform::SupervisedProcess::requestForceStopAll() !=
                    ssa::platform::ForceStopRequestStatus::Failed);
            QVERIFY(forceStopTimer.elapsed() < 50);
            future.waitForFinished();
            QVERIFY(ssa::platform::SupervisedProcess::forceStopAll());
            qunsetenv("SSA_TEST_SENTINEL_PATH");

            QCOMPARE(future.result().status, ssa::ports::WorkflowStatus::Canceled);
            QVERIFY(!QFileInfo::exists(sentinelPath));
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
        return runFakeUv(arguments.mid(1), QFileInfo{arguments.first()}.absoluteFilePath());
    }
    if (arguments.size() == 3 && arguments.at(1) == QStringLiteral("--sentinel-child")) {
        return runSentinelChild(arguments.at(2));
    }
    QCoreApplication application(argc, argv);
    SamRefreshWorkflowTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "SamRefreshWorkflowTests.moc"
