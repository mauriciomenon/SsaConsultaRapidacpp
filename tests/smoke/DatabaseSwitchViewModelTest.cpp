#include "presentation/DatabaseSwitchViewModel.h"
#include "ports/IDatabaseSwitchPorts.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

    class FakeValidator final : public ssa::ports::IDatabaseValidator {
      public:
        [[nodiscard]] ssa::ports::DatabaseValidationResult
        validate(const std::filesystem::path& path,
                 const std::stop_token stopToken) const override {
            lastPath = path;
            validationThread = QThread::currentThread();
            ++calls;
            std::unique_lock lock(mutex_);
            validationStarted_ = true;
            condition_.notify_all();
            condition_.wait(lock, stopToken, [this] { return !blocked_; });
            validationCompleted_ = true;
            lock.unlock();
            condition_.notify_all();
            if (stopToken.stop_requested()) {
                stopObserved = true;
                return {ssa::ports::DatabaseValidationStatus::Canceled,
                        "Validacao do banco cancelada",
                        {}};
            }
            if (failAfterCompletion) {
                throw std::runtime_error("late validation failure");
            }
            return result;
        }

        void blockValidation() {
            const std::scoped_lock lock(mutex_);
            blocked_ = true;
            validationStarted_ = false;
            validationCompleted_ = false;
        }

        void releaseValidation() {
            {
                const std::scoped_lock lock(mutex_);
                blocked_ = false;
            }
            condition_.notify_all();
        }

        [[nodiscard]] bool
        waitForValidationCompleted(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return validationCompleted_; });
        }

        [[nodiscard]] bool waitForValidationStart(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return validationStarted_; });
        }

        mutable std::filesystem::path lastPath;
        mutable QThread* validationThread = nullptr;
        mutable std::atomic_int calls = 0;
        mutable std::atomic_bool stopObserved = false;
        mutable std::condition_variable_any condition_;
        mutable std::mutex mutex_;
        mutable bool validationStarted_ = false;
        mutable bool validationCompleted_ = false;
        bool blocked_ = false;
        bool failAfterCompletion = false;
        ssa::ports::DatabaseValidationResult result{
            ssa::ports::DatabaseValidationStatus::Valid, {}, {}};
    };

    class FakeLauncher final : public ssa::ports::IApplicationLauncher {
      public:
        [[nodiscard]] ssa::ports::ApplicationLaunchResult
        launchWithDatabase(const std::filesystem::path& path) override {
            lastPath = path;
            launchThread = QThread::currentThread();
            ++calls;
            return result;
        }

        std::filesystem::path lastPath;
        QThread* launchThread = nullptr;
        int calls = 0;
        ssa::ports::ApplicationLaunchResult result{true, {}};
    };

    class DatabaseSwitchViewModelTest final : public QObject {
        Q_OBJECT

      private slots:
        void validatesOffThreadAndSignalsOnlyAfterLaunchStarts() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            const auto path = std::filesystem::path{temporary.filePath("other.db").toStdString()};
            auto validator = std::make_shared<FakeValidator>();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DatabaseSwitchViewModel model(validator, launcher);
            QSignalSpy started(&model,
                               &ssa::presentation::DatabaseSwitchViewModel::replacementStarted);

            model.openDatabase(QUrl::fromLocalFile(QString::fromStdString(path.string())));

            QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 3000);
            QCOMPARE(validator->calls.load(), 1);
            QCOMPARE(validator->lastPath, path);
            QVERIFY(validator->validationThread != QThread::currentThread());
            QCOMPARE(launcher->calls, 1);
            QCOMPARE(launcher->lastPath, path);
            QCOMPARE(launcher->launchThread, QThread::currentThread());
            QVERIFY(!model.running());
            QVERIFY(model.errorMessage().isEmpty());
        }

        void keepsCurrentInstanceWhenValidationFails() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            validator->result = {ssa::ports::DatabaseValidationStatus::Invalid,
                                 "O banco nao contem a tabela ssa_table",
                                 {}};
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DatabaseSwitchViewModel model(validator, launcher);
            QSignalSpy started(&model,
                               &ssa::presentation::DatabaseSwitchViewModel::replacementStarted);

            model.openDatabase(QUrl::fromLocalFile(temporary.filePath("invalid.db")));

            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QCOMPARE(started.count(), 0);
            QCOMPARE(launcher->calls, 0);
            QCOMPARE(model.errorMessage(), QStringLiteral("O banco nao contem a tabela ssa_table"));
        }

        void keepsCurrentInstanceWhenReplacementCannotStart() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            auto launcher = std::make_shared<FakeLauncher>();
            launcher->result = {false, "A nova instancia nao iniciou"};
            ssa::presentation::DatabaseSwitchViewModel model(validator, launcher);
            QSignalSpy started(&model,
                               &ssa::presentation::DatabaseSwitchViewModel::replacementStarted);

            model.openDatabase(QUrl::fromLocalFile(temporary.filePath("valid.db")));

            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QCOMPARE(started.count(), 0);
            QCOMPARE(launcher->calls, 1);
            QCOMPARE(model.errorMessage(), QStringLiteral("A nova instancia nao iniciou"));
        }

        void rejectsNonLocalUrlsAndConcurrentRequests() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DatabaseSwitchViewModel model(validator, launcher);

            model.openDatabase(QUrl{"https://example.invalid/database.db"});
            QCOMPARE(validator->calls.load(), 0);
            QCOMPARE(model.errorMessage(), QStringLiteral("Selecione um arquivo de banco local"));

            validator->blockValidation();
            const auto firstPath = temporary.filePath("first.db");
            model.openDatabase(QUrl::fromLocalFile(firstPath));
            QVERIFY(model.running());
            QVERIFY(validator->waitForValidationStart(std::chrono::seconds{1}));
            model.openDatabase(QUrl::fromLocalFile(temporary.filePath("second.db")));
            QCOMPARE(validator->calls.load(), 1);
            validator->releaseValidation();
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QCOMPARE(launcher->calls, 1);
            QCOMPARE(launcher->lastPath, std::filesystem::path{firstPath.toStdString()});
        }

        void shutdownDiscardsAnInFlightValidationWithoutLaunching() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            validator->blockValidation();
            auto launcher = std::make_shared<FakeLauncher>();
            auto model =
                std::make_unique<ssa::presentation::DatabaseSwitchViewModel>(validator, launcher);
            QSignalSpy started(model.get(),
                               &ssa::presentation::DatabaseSwitchViewModel::replacementStarted);
            model->openDatabase(QUrl::fromLocalFile(temporary.filePath("valid.db")));
            QTRY_VERIFY_WITH_TIMEOUT(model->running(), 1000);

            QElapsedTimer shutdownTimer;
            shutdownTimer.start();
            model.reset();

            QVERIFY(shutdownTimer.elapsed() < 500);
            QTRY_VERIFY_WITH_TIMEOUT(validator->stopObserved.load(), 1000);
            QCOMPARE(started.count(), 0);
            QCOMPARE(launcher->calls, 0);
        }

        void cancelIsImmediateIdempotentAndTerminalOnlyAfterValidationStops() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            validator->blockValidation();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DatabaseSwitchViewModel model(validator, launcher);

            model.openDatabase(QUrl::fromLocalFile(temporary.filePath("valid.db")));
            QTRY_VERIFY_WITH_TIMEOUT(model.running(), 1000);
            QVERIFY(model.canCancel());

            QElapsedTimer timer;
            timer.start();
            model.cancel();
            model.cancel();

            QVERIFY(timer.elapsed() < 50);
            QVERIFY(model.running());
            QVERIFY(model.canceling());
            QVERIFY(!model.canCancel());
            QTRY_VERIFY_WITH_TIMEOUT(validator->stopObserved.load(), 1000);
            QCOMPARE(launcher->calls, 0);

            validator->releaseValidation();
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QVERIFY(!model.canceling());
            QVERIFY(!model.canCancel());
            QVERIFY(model.errorMessage().isEmpty());
            QCOMPARE(launcher->calls, 0);
        }

        void cancelAfterValidationBeforeTerminalDoesNotLaunchReplacement() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DatabaseSwitchViewModel model(validator, launcher);

            model.openDatabase(QUrl::fromLocalFile(temporary.filePath("valid.db")));
            QVERIFY(validator->waitForValidationCompleted(std::chrono::seconds{1}));
            QVERIFY(model.running());

            model.cancel();

            QVERIFY(model.canceling());
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 1000);
            QCOMPARE(launcher->calls, 0);
            QVERIFY(model.errorMessage().isEmpty());
        }

        void cancelAfterFailedValidationBeforeTerminalLogsAndDoesNotLaunch() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            validator->failAfterCompletion = true;
            auto launcher = std::make_shared<FakeLauncher>();
            ssa::presentation::DatabaseSwitchViewModel model(validator, launcher);

            model.openDatabase(QUrl::fromLocalFile(temporary.filePath("valid.db")));
            QVERIFY(validator->waitForValidationCompleted(std::chrono::seconds{1}));
            QTest::ignoreMessage(QtWarningMsg, "Database validation failed after cancellation: "
                                               "late validation failure");

            model.cancel();

            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 1000);
            QCOMPARE(launcher->calls, 0);
            QVERIFY(model.errorMessage().isEmpty());
        }
    };

} // namespace

QTEST_GUILESS_MAIN(DatabaseSwitchViewModelTest)

#include "DatabaseSwitchViewModelTest.moc"
