#include "DesktopLogSink.h"

#include "presentation/RecentLogModel.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTest>

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
                    temporaryDirectory.path().toStdString(), model);
                qWarning().noquote() << "desktop log sink first warning";
                QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);
            }

            qWarning().noquote() << "desktop log sink warning after destruction";
            QCoreApplication::processEvents();
            QCOMPARE(model.rowCount(), 1);
        }

        void destructionWaitsForInFlightMessageHandler() {
            QTemporaryDir temporaryDirectory;
            QVERIFY(temporaryDirectory.isValid());
            ssa::presentation::RecentLogModel model;
            BlockingPreviousHandler blockingHandler;
            ScopedBlockingHandler installedHandler(blockingHandler);
            auto sink = std::make_unique<ssa::app::desktop::DesktopLogSink>(
                temporaryDirectory.path().toStdString(), model);

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
