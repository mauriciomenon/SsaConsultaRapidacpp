#include "presentation/MainViewModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace {

    class FakeRepository final : public ssa::ports::ISsaRepository {
      public:
        explicit FakeRepository(std::chrono::milliseconds delay = std::chrono::milliseconds{0})
            : delay_(delay) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request) const override {
            if (delay_.count() > 0) {
                std::this_thread::sleep_for(delay_);
            }
            {
                const std::scoped_lock lock(mutex_);
                requests_.push_back(request);
            }
            ssa::domain::SsaRecord record;
            record.values["numero_ssa"] = "202500001";
            record.values["situacao"] = "APV";
            record.values["descricao_ssa"] =
                request.searchText.empty() ? "Inicial" : request.searchText;
            return {{record}, 1, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&) const override {
            return 1;
        }

        std::optional<ssa::domain::SsaRecord> recordById(const ssa::domain::SsaId&) const override {
            return std::nullopt;
        }

        std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest&) const override {
            return {};
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

      private:
        std::chrono::milliseconds delay_;
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::SsaPageRequest> requests_;
    };

    class FakeCommands final : public ssa::ports::IExternalCommandPort {
      public:
        void openSamHome() override {}
        void openSsa(const std::string&) override {}
        void openPath(const std::string&) override {}
        void exportSelection(const std::vector<std::map<std::string, std::string>>&) override {}
        void requestCommand(const std::string&,
                            const std::map<std::string, std::string>&) override {}
    };

    class FakePreferences final : public ssa::ports::IUserPreferencesStore {
      public:
        explicit FakePreferences(ssa::ports::UserPreferencesSnapshot initial = {})
            : snapshot_(std::move(initial)) {}

        ssa::ports::UserPreferencesSnapshot load() const override {
            return snapshot_;
        }

        void save(const ssa::ports::UserPreferencesSnapshot& snapshot) const override {
            snapshot_ = snapshot;
            ++saveCount_;
        }

        [[nodiscard]] ssa::ports::UserPreferencesSnapshot snapshot() const {
            return snapshot_;
        }

        [[nodiscard]] int saveCount() const {
            return saveCount_;
        }

      private:
        mutable ssa::ports::UserPreferencesSnapshot snapshot_;
        mutable int saveCount_{0};
    };

    class PresentationSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void load_populates_table_and_details() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QSignalSpy pageSpy(&model, &ssa::presentation::MainViewModel::pageChanged);

            model.search()->setText("Teste");
            model.apply();

            QTRY_COMPARE_WITH_TIMEOUT(model.tableModel()->rowCount(), 1, 1000);
            QCOMPARE(model.totalRows(), 1);
            QCOMPARE(model.details()->selectedSsa(), QString("202500001"));
            QVERIFY(pageSpy.count() >= 1);
            QCOMPARE(model.status()->loading(), false);
            QCOMPARE(model.tableModel()->columnLabel(0), QString("SSA"));
            QVERIFY(model.tableModel()->columnWidth(0) > 0);
        }

        void sort_by_column_updates_request_contract() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.load();
            QTRY_COMPARE_WITH_TIMEOUT(model.tableModel()->rowCount(), 1, 1000);
            model.sortByColumn(1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);

            const auto requests = repository->requests();
            QCOMPARE(QString::fromStdString(requests.back().sort.columnKey), QString("situacao"));
            QCOMPARE(requests.back().sort.ascending, true);
            QCOMPARE(model.sortColumnKey(), QString("situacao"));
        }

        void cancel_marks_current_request_as_stale() {
            auto repository = std::make_shared<FakeRepository>(std::chrono::milliseconds{80});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.search()->setText("Primeira");
            model.apply();
            model.cancelCurrentRequest();

            QTest::qWait(160);
            QCOMPARE(model.tableModel()->rowCount(), 0);
            QCOMPARE(model.status()->message(), QString("Consulta cancelada"));
        }

        void column_settings_update_visible_columns_and_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.columns()->setColumnVisible(2, false);
            model.columns()->setColumnWidth("numero_ssa", 180);
            model.applyColumnSettings();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.tableModel()->rowCount(), 1, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(request.visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(request.visibleColumns.front()), QString("numero_ssa"));
            QCOMPARE(preferences->snapshot().columnWidths.at("numero_ssa"), 180);
            QCOMPARE(model.tableModel()->columnWidth(0), 180);
        }

        void theme_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.setTheme("dark");

            QCOMPARE(model.theme(), QString("dark"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().theme), QString("dark"));
            QCOMPARE(preferences->saveCount(), 1);
        }

        void column_settings_discard_restores_applied_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.columns()->setColumnVisible(2, false);
            model.columns()->setColumnWidth("numero_ssa", 220);
            model.discardColumnSettings();
            model.applyColumnSettings();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(request.visibleColumns.size(), std::size_t{2});
            QCOMPARE(preferences->snapshot().columnWidths.at("numero_ssa"), 140);
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationSmokeTest)

#include "PresentationSmokeTest.moc"
