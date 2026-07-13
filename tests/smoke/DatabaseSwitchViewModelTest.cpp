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
#include <filesystem>
#include <memory>
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
            while (blocked.load() && !stopToken.stop_requested()) {
                QThread::msleep(1);
            }
            if (stopToken.stop_requested()) {
                stopObserved = true;
                return {false, "Validacao do banco cancelada"};
            }
            return result;
        }

        mutable std::filesystem::path lastPath;
        mutable QThread* validationThread = nullptr;
        mutable std::atomic_int calls = 0;
        mutable std::atomic_bool stopObserved = false;
        std::atomic_bool blocked = false;
        ssa::ports::DatabaseValidationResult result{true, {}};
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
            validator->result = {false, "O banco nao contem a tabela ssa_table"};
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

            validator->blocked = true;
            const auto firstPath = temporary.filePath("first.db");
            model.openDatabase(QUrl::fromLocalFile(firstPath));
            QTRY_VERIFY_WITH_TIMEOUT(model.running(), 1000);
            model.openDatabase(QUrl::fromLocalFile(temporary.filePath("second.db")));
            QTest::qWait(20);
            QCOMPARE(validator->calls.load(), 1);
            validator->blocked = false;
            QTRY_VERIFY_WITH_TIMEOUT(!model.running(), 3000);
            QCOMPARE(launcher->calls, 1);
            QCOMPARE(launcher->lastPath, std::filesystem::path{firstPath.toStdString()});
        }

        void shutdownDiscardsAnInFlightValidationWithoutLaunching() {
            const QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            auto validator = std::make_shared<FakeValidator>();
            validator->blocked = true;
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
            QVERIFY(validator->stopObserved.load());
            QCOMPARE(started.count(), 0);
            QCOMPARE(launcher->calls, 0);
        }
    };

} // namespace

QTEST_GUILESS_MAIN(DatabaseSwitchViewModelTest)

#include "DatabaseSwitchViewModelTest.moc"
