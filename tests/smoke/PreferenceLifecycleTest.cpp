#include "PresentationSmokeFakes.h"

#include "presentation/MainPreferenceFlowCoordinator.h"
#include "presentation/MainViewModel.h"
#include "presentation/UserPreferencesCoordinator.h"
#include "query/SsaQueryService.h"

#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace {

    using ssa::tests::presentation_smoke::FakeCommands;
    using ssa::tests::presentation_smoke::FakePreferences;
    using ssa::tests::presentation_smoke::FakeRepository;

    class CompletedFilterPresetStore final : public ssa::ports::IFilterPresetStore {
      public:
        ssa::ports::FilterPresetSnapshot load(std::filesystem::path,
                                              std::stop_token = {}) const override {
            {
                const std::scoped_lock lock(mutex_);
                loadCompleted_ = true;
            }
            loadCompletedCondition_.notify_all();
            return {};
        }

        void save(std::filesystem::path, const ssa::ports::FilterPresetSnapshot&,
                  std::stop_token = {}) const override {}

        [[nodiscard]] bool waitForLoadCompletion(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return loadCompletedCondition_.wait_for(lock, timeout,
                                                    [this] { return loadCompleted_; });
        }

      private:
        mutable std::mutex mutex_;
        mutable std::condition_variable loadCompletedCondition_;
        mutable bool loadCompleted_ = false;
    };

    class NonStandardThrowingPresetStore final : public ssa::ports::IFilterPresetStore {
      public:
        ssa::ports::FilterPresetSnapshot load(std::filesystem::path,
                                              std::stop_token = {}) const override {
            throw 41;
        }

        void save(std::filesystem::path, const ssa::ports::FilterPresetSnapshot&,
                  std::stop_token = {}) const override {
            throw 42;
        }
    };

    class BlockingFilterPresetStore final : public ssa::ports::IFilterPresetStore {
      public:
        ssa::ports::FilterPresetSnapshot load(std::filesystem::path,
                                              const std::stop_token stopToken = {}) const override {
            std::unique_lock lock(mutex_);
            importStarted_ = true;
            condition_.notify_all();
            condition_.wait(lock, [this] { return releaseImport_; });
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
            return {};
        }

        void save(std::filesystem::path, const ssa::ports::FilterPresetSnapshot&,
                  const std::stop_token stopToken = {}) const override {
            std::unique_lock lock(mutex_);
            exportStarted_ = true;
            condition_.notify_all();
            condition_.wait(lock, [this] { return releaseExport_; });
            if (stopToken.stop_requested()) {
                throw std::system_error(std::make_error_code(std::errc::operation_canceled));
            }
        }

        [[nodiscard]] bool waitForImportStart(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return importStarted_; });
        }

        [[nodiscard]] bool waitForExportStart(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return exportStarted_; });
        }

        void releaseImport() const {
            {
                const std::scoped_lock lock(mutex_);
                releaseImport_ = true;
            }
            condition_.notify_all();
        }

        void releaseExport() const {
            {
                const std::scoped_lock lock(mutex_);
                releaseExport_ = true;
            }
            condition_.notify_all();
        }

      private:
        mutable std::mutex mutex_;
        mutable std::condition_variable condition_;
        mutable bool importStarted_ = false;
        mutable bool exportStarted_ = false;
        mutable bool releaseImport_ = false;
        mutable bool releaseExport_ = false;
    };

    class BlockingPreferencesStore final : public ssa::ports::IUserPreferencesStore {
      public:
        ssa::ports::UserPreferencesSnapshot load(std::stop_token = {}) const override {
            return {};
        }

        void save(const ssa::ports::UserPreferencesSnapshot& snapshot,
                  std::stop_token = {}) const override {
            std::unique_lock lock(mutex_);
            const auto call = ++saveCalls_;
            if (call == 1) {
                firstSaveStarted_ = true;
                condition_.notify_all();
                condition_.wait(lock, [this] { return releaseFirstSave_; });
            }
            savedDensities_.push_back(snapshot.density);
        }

        [[nodiscard]] bool waitForFirstSave(const std::chrono::milliseconds timeout) const {
            std::unique_lock lock(mutex_);
            return condition_.wait_for(lock, timeout, [this] { return firstSaveStarted_; });
        }

        void releaseFirstSave() const {
            {
                const std::scoped_lock lock(mutex_);
                releaseFirstSave_ = true;
            }
            condition_.notify_all();
        }

        [[nodiscard]] std::vector<std::string> savedDensities() const {
            const std::scoped_lock lock(mutex_);
            return savedDensities_;
        }

      private:
        mutable std::mutex mutex_;
        mutable std::condition_variable condition_;
        mutable std::vector<std::string> savedDensities_;
        mutable int saveCalls_ = 0;
        mutable bool firstSaveStarted_ = false;
        mutable bool releaseFirstSave_ = false;
    };

    class PreferenceLifecycleTest final : public QObject {
        Q_OBJECT

      private slots:
        void save_failure_reports_store_error() {
            auto preferences = std::make_shared<FakePreferences>();
            preferences->failNextSave("disk full");
            ssa::presentation::UserPreferencesCoordinator coordinator(preferences);
            QSignalSpy failedSpy(&coordinator,
                                 &ssa::presentation::UserPreferencesCoordinator::saveFailed);

            coordinator.saveNowOrSchedule({});

            QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 1000);
            QCOMPARE(failedSpy.takeFirst().at(0).toString(),
                     QString("Falha ao salvar preferencias"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void save_rejects_wrong_thread_call_without_saving() {
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::UserPreferencesCoordinator coordinator(preferences);
            QSignalSpy failedSpy(&coordinator,
                                 &ssa::presentation::UserPreferencesCoordinator::saveFailed);

            std::thread worker([&coordinator] { coordinator.saveNowOrSchedule({}); });
            worker.join();

            QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 1000);
            QVERIFY(failedSpy.takeFirst().at(0).toString().contains("thread"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void main_view_model_graceful_shutdown_flushes_latest_snapshot() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();

            {
                ssa::presentation::MainViewModel model(service, commands, preferences);
                model.ui()->setDensity("comfortable");
                model.requestShutdown();
                QTRY_VERIFY_WITH_TIMEOUT(model.shutdownReady(), 1000);
            }

            QCOMPARE(preferences->saveCount(), 1);
            QCOMPARE(QString::fromStdString(preferences->snapshot().density),
                     QString("comfortable"));
        }

        void graceful_shutdown_persists_latest_snapshot_without_blocking() {
            auto preferences = std::make_shared<BlockingPreferencesStore>();
            ssa::presentation::UserPreferencesCoordinator coordinator(preferences);
            QSignalSpy savedSpy(&coordinator,
                                &ssa::presentation::UserPreferencesCoordinator::saved);
            QSignalSpy shutdownSpy(
                &coordinator, &ssa::presentation::UserPreferencesCoordinator::shutdownFinished);
            ssa::ports::UserPreferencesSnapshot first;
            first.density = "first";
            ssa::ports::UserPreferencesSnapshot latest;
            latest.density = "latest";

            coordinator.saveNowOrSchedule(first);
            QVERIFY(preferences->waitForFirstSave(std::chrono::seconds{1}));
            coordinator.saveNowOrSchedule(latest);
            std::thread releaser([preferences] {
                std::this_thread::sleep_for(std::chrono::milliseconds{20});
                preferences->releaseFirstSave();
            });

            coordinator.beginShutdown(latest);
            releaser.join();
            QTRY_COMPARE_WITH_TIMEOUT(shutdownSpy.size(), 1, 1000);

            QCOMPARE(preferences->savedDensities(), std::vector<std::string>({"first", "latest"}));
            QCOMPARE(savedSpy.size(), 0);
        }

        void shutdown_without_valid_snapshot_preserves_preferences_file() {
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::UserPreferencesCoordinator coordinator(preferences);
            QSignalSpy shutdownSpy(
                &coordinator, &ssa::presentation::UserPreferencesCoordinator::shutdownFinished);

            coordinator.beginShutdown(std::nullopt);

            QTRY_COMPARE_WITH_TIMEOUT(shutdownSpy.size(), 1, 1000);
            QCOMPARE(preferences->saveCount(), 0);
        }

        void preference_failure_exposes_safe_message_only() {
            auto preferences = std::make_shared<FakePreferences>();
            preferences->failNextSave("sqlite path=/private/preferences.json rc=13");
            ssa::presentation::UserPreferencesCoordinator coordinator(preferences);
            QSignalSpy failedSpy(&coordinator,
                                 &ssa::presentation::UserPreferencesCoordinator::saveFailed);

            coordinator.saveNowOrSchedule({});

            QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 1000);
            const auto message = failedSpy.takeFirst().at(0).toString();
            QCOMPARE(message, QString("Falha ao salvar preferencias"));
            QVERIFY(!message.contains("/private"));
            QVERIFY(!message.contains("rc=13"));
        }

        void preset_operation_is_contextually_cancelable_until_terminal() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto presets = std::make_shared<BlockingFilterPresetStore>();
            ssa::presentation::MainViewModel model(service, commands, nullptr, presets);

            QVERIFY(QMetaObject::invokeMethod(
                model.preferenceFlow(), "exportFilterPreset", Qt::DirectConnection,
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json"))));
            QVERIFY(presets->waitForExportStart(std::chrono::seconds{1}));
            QVERIFY(model.canCancelActivity());

            model.requestCancelAll();

            QVERIFY(model.cancelingActivity());
            QVERIFY(!model.canCancelActivity());
            presets->releaseExport();
            QTRY_VERIFY_WITH_TIMEOUT(!model.cancelingActivity(), 1000);

            QVERIFY(QMetaObject::invokeMethod(
                model.preferenceFlow(), "importFilterPreset", Qt::DirectConnection,
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json"))));
            QVERIFY(presets->waitForImportStart(std::chrono::seconds{1}));
            QVERIFY(model.canCancelActivity());

            model.requestCancelAll();

            QVERIFY(model.cancelingActivity());
            QVERIFY(!model.canCancelActivity());
            presets->releaseImport();
            QTRY_VERIFY_WITH_TIMEOUT(!model.cancelingActivity(), 1000);
        }

        void destruction_waits_for_active_save_without_callback() {
            auto preferences = std::make_shared<BlockingPreferencesStore>();
            auto coordinator =
                std::make_unique<ssa::presentation::UserPreferencesCoordinator>(preferences);
            QSignalSpy savedSpy(coordinator.get(),
                                &ssa::presentation::UserPreferencesCoordinator::saved);
            QSignalSpy failedSpy(coordinator.get(),
                                 &ssa::presentation::UserPreferencesCoordinator::saveFailed);
            ssa::ports::UserPreferencesSnapshot snapshot;
            snapshot.density = "active";

            coordinator->saveNowOrSchedule(snapshot);
            QVERIFY(preferences->waitForFirstSave(std::chrono::seconds{1}));
            std::mutex releaseMutex;
            std::condition_variable releaseCondition;
            bool destructionCallbackEntered = false;
            std::atomic_bool destructionFinished = false;
            std::thread releaser([&] {
                std::unique_lock lock(releaseMutex);
                releaseCondition.wait(lock, [&] { return destructionCallbackEntered; });
                preferences->releaseFirstSave();
            });
            connect(coordinator.get(),
                    &ssa::presentation::UserPreferencesCoordinator::shutdownStarted, this, [&] {
                        {
                            const std::scoped_lock lock(releaseMutex);
                            destructionCallbackEntered = true;
                        }
                        releaseCondition.notify_one();
                    });
            const auto coordinatorHolder =
                std::make_shared<decltype(coordinator)>(std::move(coordinator));
            QTimer::singleShot(0, QCoreApplication::instance(), [&, coordinatorHolder] {
                coordinatorHolder->reset();
                destructionFinished.store(true, std::memory_order_release);
            });

            QTRY_VERIFY_WITH_TIMEOUT(destructionFinished.load(std::memory_order_acquire), 1000);
            releaser.join();
            QCoreApplication::processEvents();

            QCOMPARE(preferences->savedDensities(), std::vector<std::string>({"active"}));
            QCOMPARE(savedSpy.size(), 0);
            QCOMPARE(failedSpy.size(), 0);
        }

        void preset_completion_is_not_delivered_during_destruction() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto presets = std::make_shared<CompletedFilterPresetStore>();
            auto model = std::make_unique<ssa::presentation::MainViewModel>(service, commands,
                                                                            nullptr, presets);
            QSignalSpy statusSpy(model->preferenceFlow(), SIGNAL(statusMessageRequested(QString)));

            QMetaObject::invokeMethod(
                model->preferenceFlow(), "importFilterPreset", Qt::DirectConnection,
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json")));
            QVERIFY(presets->waitForLoadCompletion(std::chrono::seconds{1}));
            QCOMPARE(statusSpy.size(), 1);

            model.reset();

            QCOMPARE(statusSpy.size(), 1);
        }

        void preset_export_destruction_waits_without_terminal_callback() {
            runBlockingPresetDestruction(true);
        }

        void preset_import_destruction_waits_without_terminal_callback() {
            runBlockingPresetDestruction(false);
        }

        void preset_non_standard_export_exception_is_reported() {
            verifyNonStandardPresetException(true);
        }

        void preset_non_standard_import_exception_is_reported() {
            verifyNonStandardPresetException(false);
        }

      private:
        void runBlockingPresetDestruction(const bool exportPreset) {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto presets = std::make_shared<BlockingFilterPresetStore>();
            auto model = std::make_unique<ssa::presentation::MainViewModel>(service, commands,
                                                                            nullptr, presets);
            QSignalSpy statusSpy(model->preferenceFlow(), SIGNAL(statusMessageRequested(QString)));
            QSignalSpy errorSpy(model->preferenceFlow(), SIGNAL(statusErrorRequested(QString)));
            const char* method = exportPreset ? "exportFilterPreset" : "importFilterPreset";

            QVERIFY(QMetaObject::invokeMethod(
                model->preferenceFlow(), method, Qt::DirectConnection,
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json"))));
            if (exportPreset) {
                QVERIFY(presets->waitForExportStart(std::chrono::seconds{1}));
            } else {
                QVERIFY(presets->waitForImportStart(std::chrono::seconds{1}));
            }
            QCOMPARE(statusSpy.size(), 1);

            std::mutex releaseMutex;
            std::condition_variable releaseCondition;
            bool destructionCallbackEntered = false;
            std::atomic_bool destructionFinished = false;
            std::thread releaser([&] {
                std::unique_lock lock(releaseMutex);
                releaseCondition.wait(lock, [&] { return destructionCallbackEntered; });
                if (exportPreset) {
                    presets->releaseExport();
                } else {
                    presets->releaseImport();
                }
            });
            auto* preferenceFlow = qobject_cast<ssa::presentation::MainPreferenceFlowCoordinator*>(
                model->preferenceFlow());
            QVERIFY(preferenceFlow != nullptr);
            connect(preferenceFlow,
                    &ssa::presentation::MainPreferenceFlowCoordinator::shutdownStarted, this, [&] {
                        {
                            const std::scoped_lock lock(releaseMutex);
                            destructionCallbackEntered = true;
                        }
                        releaseCondition.notify_one();
                    });
            const auto modelHolder = std::make_shared<decltype(model)>(std::move(model));
            QTimer::singleShot(0, QCoreApplication::instance(), [&, modelHolder] {
                modelHolder->reset();
                destructionFinished.store(true, std::memory_order_release);
            });

            QTRY_VERIFY_WITH_TIMEOUT(destructionFinished.load(std::memory_order_acquire), 1000);
            releaser.join();
            QCoreApplication::processEvents();

            QCOMPARE(statusSpy.size(), 1);
            QCOMPARE(errorSpy.size(), 0);
        }

        void verifyNonStandardPresetException(const bool exportPreset) {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto presets = std::make_shared<NonStandardThrowingPresetStore>();
            ssa::presentation::MainViewModel model(service, commands, nullptr, presets);
            QSignalSpy errorSpy(model.preferenceFlow(), SIGNAL(statusErrorRequested(QString)));
            const char* method = exportPreset ? "exportFilterPreset" : "importFilterPreset";

            QVERIFY(QMetaObject::invokeMethod(
                model.preferenceFlow(), method, Qt::DirectConnection,
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json"))));

            QTRY_COMPARE_WITH_TIMEOUT(errorSpy.size(), 1, 1000);
            const auto expected = exportPreset ? QString("Falha ao exportar filtros")
                                               : QString("Falha ao importar filtros");
            QCOMPARE(errorSpy.takeFirst().at(0).toString(), expected);
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PreferenceLifecycleTest)

#include "PreferenceLifecycleTest.moc"
