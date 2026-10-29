#include "DesktopLogSink.h"

#include "presentation/RecentLogModel.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace {

    class BlockingPreviousHandler final {
      public:
        void block() {
            std::unique_lock lock(mutex_);
            entered_ = true;
            condition_.notify_all();
            condition_.wait(lock, [this] { return released_; });
        }

        [[nodiscard]] bool waitUntilEntered(const std::chrono::milliseconds timeout) {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return entered_; });
        }

        void release() {
            {
                const std::scoped_lock lock(mutex_);
                released_ = true;
            }
            condition_.notify_all();
        }

      private:
        std::mutex mutex_;
        std::condition_variable condition_;
        bool entered_{false};
        bool released_{false};
    };

    BlockingPreviousHandler* activeBlockingHandler = nullptr;

    void blockingMessageHandler(QtMsgType, const QMessageLogContext&, const QString&) {
        activeBlockingHandler->block();
    }

    class ScopedBlockingHandler final {
      public:
        explicit ScopedBlockingHandler(BlockingPreviousHandler& handler) : handler_(handler) {
            activeBlockingHandler = &handler_;
            previousHandler_ = qInstallMessageHandler(&blockingMessageHandler);
        }

        ~ScopedBlockingHandler() {
            qInstallMessageHandler(previousHandler_);
            activeBlockingHandler = nullptr;
        }

        ScopedBlockingHandler(const ScopedBlockingHandler&) = delete;
        ScopedBlockingHandler& operator=(const ScopedBlockingHandler&) = delete;

      private:
        BlockingPreviousHandler& handler_;
        QtMessageHandler previousHandler_{nullptr};
    };

    class ReleaseBlockingHandler final {
      public:
        explicit ReleaseBlockingHandler(BlockingPreviousHandler& handler) : handler_(handler) {}

        ~ReleaseBlockingHandler() {
            handler_.release();
        }

      private:
        BlockingPreviousHandler& handler_;
    };

    class DesktopLogSinkTest final : public QObject {
        Q_OBJECT

      private slots:
        void destructionStopsPublishingNewMessages() {
            QTemporaryDir temporaryDirectory;
            QVERIFY(temporaryDirectory.isValid());
            ssa::presentation::RecentLogModel model;

            {
                auto sink = std::make_unique<ssa::app::desktop::DesktopLogSink>(
                    temporaryDirectory.path().toStdString());
                sink->attachModel(model);
                qWarning().noquote() << "desktop log sink first warning";
                QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);
            }

            qWarning().noquote() << "desktop log sink warning after destruction";
            QCoreApplication::processEvents();
            QCOMPARE(model.rowCount(), 1);
        }

        void startupMessagesReachTheFileAndTheModelAttachedLater() {
            QTemporaryDir temporaryDirectory;
            QVERIFY(temporaryDirectory.isValid());
            ssa::app::desktop::DesktopLogSink sink(temporaryDirectory.path().toStdString());

            qWarning().noquote() << "desktop log sink startup warning";
            QFile logFile(temporaryDirectory.filePath(QStringLiteral("logs/ssa.log")));
            QVERIFY(logFile.open(QIODevice::ReadOnly));
            QVERIFY(logFile.readAll().contains("desktop log sink startup warning"));

            ssa::presentation::RecentLogModel model;
            sink.attachModel(model);
            QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);
            qWarning().noquote() << "desktop log sink attached warning";
            QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, 1000);
        }

        void infoMessagesReachOnlyTheFile() {
            QTemporaryDir temporaryDirectory;
            QVERIFY(temporaryDirectory.isValid());
            ssa::app::desktop::DesktopLogSink sink(temporaryDirectory.path().toStdString());
            ssa::presentation::RecentLogModel model;
            sink.attachModel(model);

            qInfo().noquote() << "desktop log sink journal info";
            qWarning().noquote() << "desktop log sink journal warning";
            QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);
            QCoreApplication::processEvents();
            QCOMPARE(model.rowCount(), 1);

            QFile logFile(temporaryDirectory.filePath(QStringLiteral("logs/ssa.log")));
            QVERIFY(logFile.open(QIODevice::ReadOnly));
            const auto contents = logFile.readAll();
            QVERIFY2(contents.contains("[Info] desktop log sink journal info"),
                     contents.constData());
            QVERIFY(contents.contains("[Warning] desktop log sink journal warning"));
        }

        void detachedModelIsSafeToDestroyWhileWorkersLog() {
            QTemporaryDir temporaryDirectory;
            QVERIFY(temporaryDirectory.isValid());
            ssa::app::desktop::DesktopLogSink sink(temporaryDirectory.path().toStdString());
            auto model = std::make_unique<ssa::presentation::RecentLogModel>();
            sink.attachModel(*model);
            qWarning().noquote() << "desktop log sink before detach";
            QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(), 1, 1000);

            // Shutdown: a worker keeps logging while the view model (and its list) is destroyed.
            std::atomic_bool stop{false};
            std::jthread worker([&stop] {
                while (!stop.load()) {
                    qWarning().noquote() << "desktop log sink worker during shutdown";
                }
            });
            sink.detachModel();
            QCOMPARE(model->rowCount(), 1);
            model.reset();
            QCoreApplication::processEvents();
            stop = true;
            worker.join();
            QCoreApplication::processEvents();
        }

        void destructionWaitsForInFlightMessageHandler() {
            QTemporaryDir temporaryDirectory;
            QVERIFY(temporaryDirectory.isValid());
            ssa::presentation::RecentLogModel model;
            BlockingPreviousHandler blockingHandler;
            ScopedBlockingHandler installedHandler(blockingHandler);
            auto sink = std::make_unique<ssa::app::desktop::DesktopLogSink>(
                temporaryDirectory.path().toStdString());
            sink->attachModel(model);

            std::jthread warningThread(
                [] { qWarning().noquote() << "desktop log sink blocking warning"; });
            ReleaseBlockingHandler releaseBlockingHandler(blockingHandler);
            QVERIFY(blockingHandler.waitUntilEntered(std::chrono::seconds{1}));

            std::promise<void> destructionPromise;
            const auto destructionFinished = destructionPromise.get_future();
            std::jthread destructionThread(
                [&sink, promise = std::move(destructionPromise)]() mutable {
                    sink.reset();
                    promise.set_value();
                });

            QVERIFY(destructionFinished.wait_for(std::chrono::milliseconds{50}) ==
                    std::future_status::timeout);
            blockingHandler.release();
            QVERIFY(destructionFinished.wait_for(std::chrono::seconds{1}) ==
                    std::future_status::ready);
        }
    };

} // namespace

QTEST_GUILESS_MAIN(DesktopLogSinkTest)

#include "DesktopLogSinkTest.moc"
