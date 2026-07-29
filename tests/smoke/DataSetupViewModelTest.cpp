#include "presentation/DataSetupViewModel.h"
#include "platform/DesktopApplicationLauncher.h"
#include "platform/StartupOptions.h"
#include "ports/IDatabaseSwitchPorts.h"
#include "qt/FilesystemPath.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QVariant>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

namespace {

    class FakeDataSetupPort final : public ssa::ports::IDataSetupPort {
      public:
        [[nodiscard]] ssa::ports::DataSetupResult
        execute(const ssa::ports::DataSetupRequest& request,
                const std::stop_token stopToken) override {
            lastRequest = request;
            executionThread = QThread::currentThread();
            ++calls;
            std::unique_lock lock(mutex_);
            started_ = true;
            condition_.notify_all();
            condition_.wait(lock, stopToken, [this] { return !blocked_; });
            stopObserved = stopToken.stop_requested();
            auto returned = result;
            if (returned.ok && returned.databasePath.empty()) {
                returned.databasePath = request.projectRoot / "data" / "ssas.db";
            }
            completed_ = true;
            lock.unlock();
            condition_.notify_all();
            return returned;
        }

        void block() {
            const std::scoped_lock lock(mutex_);
            blocked_ = true;
            started_ = false;
            completed_ = false;
        }

        [[nodiscard]] bool waitForStart(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return started_; });
        }

        [[nodiscard]] bool waitForCompletion(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return completed_; });
        }

        ssa::ports::DataSetupRequest lastRequest;
        QThread* executionThread = nullptr;
        std::atomic_int calls = 0;
        std::atomic_bool stopObserved = false;
        ssa::ports::DataSetupResult result{true, "setup complete", {}, {}};

      private:
        mutable std::mutex mutex_;
        mutable std::condition_variable_any condition_;
        bool blocked_ = false;
        bool started_ = false;
        bool completed_ = false;
    };

    class FakeLauncher final : public ssa::ports::IApplicationLauncher {
      public:
        [[nodiscard]] ssa::ports::ApplicationLaunchResult
        launchWithDatabase(const std::filesystem::path&) override {
            ++databaseCalls;
            return {true, {}};
        }

        [[nodiscard]] ssa::ports::ApplicationLaunchResult
        launchConfigured(const ssa::ports::ApplicationLaunchTargets& targets) override {
            lastTargets = targets;
            launchThread = QThread::currentThread();
            ++configuredCalls;
            if (configuredCalls == 1 && firstConfiguredResult) {
                return *firstConfiguredResult;
            }
            return result;
        }

        ssa::ports::ApplicationLaunchTargets lastTargets;
        QThread* launchThread = nullptr;
        int databaseCalls = 0;
        int configuredCalls = 0;
        std::optional<ssa::ports::ApplicationLaunchResult> firstConfiguredResult;
        ssa::ports::ApplicationLaunchResult result{true, {}};
    };

    class DataSetupViewModelTest final : public QObject {
        Q_OBJECT

      private slots:
        void defaultDestinationRunsOffThreadAndLaunchesConfiguredTargets() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            const auto root = ssa::qt::toFileSystemPath(temporary.path()) / "home-root";
            auto setup = std::make_shared<FakeDataSetupPort>();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DataSetupViewModel model(setup, launcher, ssa::qt::toQString(root));
            QSignalSpy replacement(&model,
                                   &ssa::presentation::DataSetupViewModel::replacementStarted);

            model.setAction(0);
            model.execute();

            QTRY_COMPARE_WITH_TIMEOUT(replacement.count(), 1, 3000);
            QCOMPARE(setup->calls.load(), 1);
            QCOMPARE(setup->lastRequest.action, ssa::ports::DataSetupAction::CreateEmpty);
            QCOMPARE(setup->lastRequest.projectRoot, root);
            QVERIFY(setup->executionThread != QThread::currentThread());
            QCOMPARE(launcher->configuredCalls, 1);
            QCOMPARE(launcher->lastTargets.databasePath, root / "data" / "ssas.db");
            QCOMPARE(launcher->lastTargets.projectRoot, root);
            QCOMPARE(launcher->lastTargets.configDir, root / "config");
            QCOMPARE(launcher->launchThread, QThread::currentThread());
            QVERIFY(!model.running());
            QVERIFY(model.errorMessage().isEmpty());
        }

        void customDestinationAndInputsReachTheSetupRequest() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            const auto root = ssa::qt::toFileSystemPath(temporary.path()) / "custom-root";
            const auto source = ssa::qt::toFileSystemPath(temporary.path()) / "source.db";
            const auto firstXlsx = ssa::qt::toFileSystemPath(temporary.path()) / "first.xlsx";
            const auto secondXlsx = ssa::qt::toFileSystemPath(temporary.path()) / "second.xlsx";
            auto setup = std::make_shared<FakeDataSetupPort>();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DataSetupViewModel model(setup, launcher,
                                                        QStringLiteral("/unused-default"));
            QSignalSpy replacement(&model,
                                   &ssa::presentation::DataSetupViewModel::replacementStarted);

            model.setDestinationMode(1);
            model.setCustomDestination(QUrl::fromLocalFile(ssa::qt::toQString(root)));
            model.setAction(3);
            model.setSourceDatabase(QUrl::fromLocalFile(ssa::qt::toQString(source)));
            model.setXlsxFiles(
                {QVariant::fromValue(QUrl::fromLocalFile(ssa::qt::toQString(firstXlsx))),
                 QVariant::fromValue(QUrl::fromLocalFile(ssa::qt::toQString(secondXlsx)))});
            model.execute();

            QTRY_COMPARE_WITH_TIMEOUT(replacement.count(), 1, 3000);
            QCOMPARE(setup->lastRequest.action, ssa::ports::DataSetupAction::ImportDatabaseAndXlsx);
            QCOMPARE(setup->lastRequest.projectRoot, root);
            QCOMPARE(setup->lastRequest.sourceDatabase, source);
            QCOMPARE(setup->lastRequest.xlsxFiles,
                     std::vector<std::filesystem::path>({firstXlsx, secondXlsx}));
            QCOMPARE(launcher->lastTargets.databasePath, root / "data" / "ssas.db");
            QCOMPARE(launcher->lastTargets.projectRoot, root);
            QCOMPARE(launcher->lastTargets.configDir, root / "config");
        }

        void setupFailureAndMissingCustomDestinationNeverLaunch() {
            auto setup = std::make_shared<FakeDataSetupPort>();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DataSetupViewModel model(setup, launcher,
                                                        QStringLiteral("/default-root"));
            QSignalSpy replacement(&model,
                                   &ssa::presentation::DataSetupViewModel::replacementStarted);

            model.setDestinationMode(1);
            model.execute();
            QCOMPARE(setup->calls.load(), 0);
            QCOMPARE(launcher->configuredCalls, 0);
            QVERIFY(!model.errorMessage().isEmpty());

            model.setDestinationMode(0);
            setup->result = {false, "setup failed", "technical failure", {}};
            model.execute();

            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QCOMPARE(setup->calls.load(), 1);
            QCOMPARE(launcher->configuredCalls, 0);
            QCOMPARE(replacement.count(), 0);
            QCOMPARE(model.errorMessage(), QStringLiteral("setup failed"));
        }

        void launcherFailureRetriesPublishedTargetWithoutRepeatingSetup() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            const auto publishedRoot =
                ssa::qt::toFileSystemPath(temporary.path()) / "published-root";
            const auto redirectedRoot =
                ssa::qt::toFileSystemPath(temporary.path()) / "redirected-root";
            auto setup = std::make_shared<FakeDataSetupPort>();
            auto launcher = std::make_shared<FakeLauncher>();
            launcher->firstConfiguredResult =
                ssa::ports::ApplicationLaunchResult{false, "replacement failed"};
            ssa::presentation::DataSetupViewModel model(setup, launcher,
                                                        ssa::qt::toQString(publishedRoot));
            QSignalSpy replacement(&model,
                                   &ssa::presentation::DataSetupViewModel::replacementStarted);

            model.execute();

            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QCOMPARE(setup->calls.load(), 1);
            QCOMPARE(launcher->configuredCalls, 1);
            QCOMPARE(replacement.count(), 0);
            QCOMPARE(model.errorMessage(), QStringLiteral("replacement failed"));

            model.setDestinationMode(1);
            model.setCustomDestination(QUrl::fromLocalFile(ssa::qt::toQString(redirectedRoot)));
            model.execute();

            QTRY_COMPARE_WITH_TIMEOUT(replacement.count(), 1, 3000);
            QCOMPARE(setup->calls.load(), 1);
            QCOMPARE(launcher->configuredCalls, 2);
            QCOMPARE(launcher->lastTargets.databasePath, publishedRoot / "data" / "ssas.db");
            QCOMPARE(launcher->lastTargets.projectRoot, publishedRoot);
            QCOMPARE(launcher->lastTargets.configDir, publishedRoot / "config");
            QVERIFY(model.errorMessage().isEmpty());
        }

        void cancellationWhosePortReturnsFailureDoesNotLaunch() {
            auto setup = std::make_shared<FakeDataSetupPort>();
            setup->block();
            setup->result = {false, "Configuracao de dados cancelada", {}, {}};
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DataSetupViewModel model(setup, launcher,
                                                        QStringLiteral("/default-root"));
            QSignalSpy replacement(&model,
                                   &ssa::presentation::DataSetupViewModel::replacementStarted);

            model.execute();
            QVERIFY(setup->waitForStart(std::chrono::seconds{1}));
            model.cancel();

            QTRY_VERIFY_WITH_TIMEOUT(setup->stopObserved.load(), 1000);
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QCOMPARE(launcher->configuredCalls, 0);
            QCOMPARE(replacement.count(), 0);
            QVERIFY(model.errorMessage().isEmpty());
            QVERIFY(model.progressMessage().isEmpty());
        }

        void cancellationAfterSuccessfulPortCompletionStillLaunchesReplacement() {
            auto setup = std::make_shared<FakeDataSetupPort>();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DataSetupViewModel model(setup, launcher,
                                                        QStringLiteral("/default-root"));
            QSignalSpy replacement(&model,
                                   &ssa::presentation::DataSetupViewModel::replacementStarted);

            model.execute();
            QVERIFY(setup->waitForCompletion(std::chrono::seconds{1}));
            QVERIFY(QThreadPool::globalInstance()->waitForDone(1000));
            QVERIFY(model.running());
            QVERIFY(!setup->stopObserved.load());

            model.cancel();

            QTRY_COMPARE_WITH_TIMEOUT(replacement.count(), 1, 1000);
            QCOMPARE(launcher->configuredCalls, 1);
            QVERIFY(model.errorMessage().isEmpty());
            QVERIFY(model.progressMessage().isEmpty());
        }

        void shutdownReturnsImmediatelyAndPreventsLatePublication() {
            auto setup = std::make_shared<FakeDataSetupPort>();
            setup->block();
            auto launcher = std::make_shared<FakeLauncher>();
            auto model = std::make_unique<ssa::presentation::DataSetupViewModel>(
                setup, launcher, QStringLiteral("/default-root"));
            QSignalSpy replacement(model.get(),
                                   &ssa::presentation::DataSetupViewModel::replacementStarted);
            model->execute();
            QVERIFY(setup->waitForStart(std::chrono::seconds{1}));

            QElapsedTimer timer;
            timer.start();
            model.reset();

            QVERIFY(timer.elapsed() < 500);
            QTRY_VERIFY_WITH_TIMEOUT(setup->stopObserved.load(), 1000);
            QCOMPARE(launcher->configuredCalls, 0);
            QCOMPARE(replacement.count(), 0);
        }

        void configuredLauncherBuildsArgumentsInTheRequiredOrder() {
            ssa::platform::StartupOptions options;
            options.projectRoot = QStringLiteral("/old-root");
            options.configDir = QStringLiteral("/old-config");
            options.samBaseUrl = QStringLiteral("https://sam.example.invalid");
            ssa::platform::DesktopApplicationLauncher launcher(options);
            const ssa::ports::ApplicationLaunchTargets targets{.databasePath =
                                                                   "/new-root/data/ssas.db",
                                                               .projectRoot = "/new-root",
                                                               .configDir = "/new-root/config"};

            QCOMPARE(
                launcher.argumentsForConfigured(targets),
                QStringList({QStringLiteral("--project-root"), QStringLiteral("/new-root"),
                             QStringLiteral("--config-dir"), QStringLiteral("/new-root/config"),
                             QStringLiteral("--sam-url"),
                             QStringLiteral("https://sam.example.invalid"), QStringLiteral("--db"),
                             QStringLiteral("/new-root/data/ssas.db")}));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(DataSetupViewModelTest)

#include "DataSetupViewModelTest.moc"
