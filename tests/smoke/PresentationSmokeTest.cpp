#include "domain/SsaTypes.h"
#include "presentation/FilterPanelViewModel.h"
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
        explicit FakeRepository(std::chrono::milliseconds delay = std::chrono::milliseconds{0},
                                std::size_t totalRows = 1)
            : delay_(delay), totalRows_(totalRows) {}

        ssa::domain::SsaPageResult page(const ssa::domain::SsaPageRequest& request) const override {
            if (delay_.count() > 0) {
                std::this_thread::sleep_for(delay_);
            }
            {
                const std::scoped_lock lock(mutex_);
                requests_.push_back(request);
            }
            const ssa::domain::SsaRecord record{
                {{"numero_ssa", "202500001"},
                 {"situacao", "APV"},
                 {"descricao_ssa", request.searchText.empty() ? "Inicial" : request.searchText}}};
            return {{record}, totalRows_, request.pageIndex, request.pageSize};
        }

        std::size_t count(const ssa::domain::SsaPageRequest&) const override {
            return 1;
        }

        std::optional<ssa::domain::SsaRecord>
        recordBySsaNumber(const ssa::domain::SsaNumber&) const override {
            return std::nullopt;
        }

        std::vector<std::string>
        distinctValues(const ssa::domain::DistinctValuesRequest&) const override {
            return {};
        }

        ssa::ports::SsaReadResult readAll(const ssa::domain::SsaPageRequest& request,
                                          ssa::ports::SsaRecordConsumer consume) const override {
            auto pageResult = page(request);
            std::size_t rowCount = 0;
            for (const auto& row : pageResult.rows) {
                if (auto error = consume(row); error.has_value()) {
                    return {rowCount, *error};
                }
                ++rowCount;
            }
            return {rowCount, {}};
        }

        [[nodiscard]] std::vector<ssa::domain::SsaPageRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

      private:
        std::chrono::milliseconds delay_;
        std::size_t totalRows_;
        mutable std::mutex mutex_;
        mutable std::vector<ssa::domain::SsaPageRequest> requests_;
    };

    class FakeCommands final : public ssa::ports::IExternalCommandPort {
      public:
        ssa::ports::ExternalCommandResult
        execute(const ssa::ports::ExternalCommand& command) override {
            const std::scoped_lock lock(mutex);
            commands_.push_back(command);
            return nextResult;
        }

        [[nodiscard]] std::vector<ssa::ports::ExternalCommand> commands() const {
            const std::scoped_lock lock(mutex);
            return commands_;
        }

        ssa::ports::ExternalCommandResult nextResult{ssa::ports::ExternalCommandStatus::Succeeded,
                                                     "ok"};

      private:
        mutable std::mutex mutex;
        std::vector<ssa::ports::ExternalCommand> commands_;
    };

    class FakePreferences final : public ssa::ports::IUserPreferencesStore {
      public:
        explicit FakePreferences(ssa::ports::UserPreferencesSnapshot initial = {})
            : snapshot_(std::move(initial)) {}

        ssa::ports::UserPreferencesSnapshot load() const override {
            const std::scoped_lock lock(mutex_);
            return snapshot_;
        }

        void save(const ssa::ports::UserPreferencesSnapshot& snapshot) const override {
            const std::scoped_lock lock(mutex_);
            snapshot_ = snapshot;
            ++saveCount_;
        }

        [[nodiscard]] ssa::ports::UserPreferencesSnapshot snapshot() const {
            const std::scoped_lock lock(mutex_);
            return snapshot_;
        }

        [[nodiscard]] int saveCount() const {
            const std::scoped_lock lock(mutex_);
            return saveCount_;
        }

      private:
        mutable ssa::ports::UserPreferencesSnapshot snapshot_;
        mutable int saveCount_{0};
        mutable std::mutex mutex_;
    };

    class PresentationSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void load_populates_table_and_allows_details_selection() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QSignalSpy pageSpy(&model, &ssa::presentation::MainViewModel::pageChanged);

            model.browse()->search()->setText("Teste");
            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QCOMPARE(model.browse()->totalRows(), 1);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString());
            model.browse()->selectRow(0);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));
            QVERIFY(pageSpy.count() >= 1);
            QCOMPARE(model.browse()->status()->loading(), false);
            QCOMPARE(model.browse()->tableModel()->columnLabel(0), QString("No SSA"));
            QVERIFY(model.browse()->tableModel()->columnWidth(0) > 0);
        }

        void sort_by_column_updates_request_contract() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.browse()->sortByColumn(1);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);

            const auto requests = repository->requests();
            QCOMPARE(QString::fromStdString(requests.back().sort.columnKey), QString("situacao"));
            QCOMPARE(requests.back().sort.ascending, true);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
        }

        void next_page_reaches_final_page() {
            auto repository =
                std::make_shared<FakeRepository>(std::chrono::milliseconds{0}, std::size_t{21});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->setPageSize(10);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageCount(), 3, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 3000);
            QCOMPARE(repository->requests().back().pageIndex, std::size_t{1});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{3}, 3000);
            QCOMPARE(repository->requests().back().pageIndex, std::size_t{2});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 3, 1000);

            QCOMPARE(model.browse()->pageNumber(), 3);
            QCOMPARE(model.browse()->pageCount(), 3);
            QCOMPARE(repository->requests().back().pageIndex, std::size_t{2});
        }

        void cancel_marks_current_request_as_stale() {
            auto repository = std::make_shared<FakeRepository>(std::chrono::milliseconds{80});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->search()->setText("Primeira");
            model.browse()->apply();
            model.browse()->cancelCurrentRequest();

            QTest::qWait(160);
            QCOMPARE(model.browse()->tableModel()->rowCount(), 0);
            QCOMPARE(model.browse()->status()->message(), QString("Consulta cancelada"));
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

            model.columns()->setColumnVisibleByKey("situacao", false);
            model.columns()->setColumnWidth("numero_ssa", 180);
            model.applyColumnSettings();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(request.visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(request.visibleColumns.front()), QString("numero_ssa"));
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 180,
                                      1000);
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 180);
        }

        void column_width_update_does_not_reload_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.columns()->setColumnWidth("numero_ssa", 220);
            model.applyColumnSettings();

            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 220);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 220,
                                      1000);
        }

        void column_filter_summary_uses_contains_marker() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("situacao");
            filters.setColumnValue("APV");
            filters.addColumnFilter();

            QCOMPARE(filters.activeFilters().contains("situacao:APV"), true);
            QCOMPARE(filters.activeFilterSummary().contains("situacao:APV"), true);
        }

        void advanced_filters_are_added_to_request_contract() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->filters()->setWeekColumnKey("semana_programada");
            model.browse()->filters()->setYearFilter("2025");
            model.browse()->filters()->setWeekFilter("2");
            model.browse()->filters()->setDerivationMode("derived");
            model.browse()->filters()->setOnlyReprogrammed(true);
            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(QString::fromStdString(request.advancedFilters.weekColumnKey),
                     QString("semana_programada"));
            QCOMPARE(request.advancedFilters.year.value_or(0), 2025);
            QCOMPARE(request.advancedFilters.week.value_or(0), 2);
            QCOMPARE(request.advancedFilters.derivationMode,
                     ssa::domain::DerivationFilterMode::DerivedOnly);
            QCOMPARE(request.advancedFilters.onlyReprogrammed, true);
        }

        void reset_filters_restores_default_sca_ses_ste_exclusion() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("setor_executor");
            filters.resetFilters();

            QCOMPARE(filters.columnKey(), QString("situacao"));
            QCOMPARE(filters.excludeScaSesSte(), true);
            QCOMPARE(filters.activeFilters().contains("sem SCA/SES/STE"), true);
        }

        void theme_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setTheme("dark");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->theme(), QString("dark"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().theme), QString("dark"));
        }

        void details_visibility_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDetailsVisible(false);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->detailsVisible(), false);
            QCOMPARE(preferences->snapshot().detailsVisible, false);
        }

        void details_panel_width_preference_is_saved_and_clamped() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDetailsPanelWidth(620);
            model.ui()->setDetailsPanelWidth(900);

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->detailsPanelWidth(), 900);
            QCOMPARE(preferences->snapshot().detailsPanelWidth, 900);
        }

        void density_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDensity("comfortable");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->density(), QString("comfortable"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().density),
                     QString("comfortable"));
        }

        void invalid_density_is_ignored() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDensity("wide");

            QCOMPARE(model.ui()->density(), QString("normal"));
            QCOMPARE(preferences->saveCount(), 0);
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

            model.columns()->setColumnVisibleByKey("situacao", false);
            model.columns()->setColumnWidth("numero_ssa", 220);
            model.discardColumnSettings();
            model.applyColumnSettings();

            QCOMPARE(repository->requests().size(), std::size_t{0});
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 140,
                                      1000);
        }

        void command_view_model_uses_external_command_port() {
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openSsa("202500001");

            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            const auto executed = commands->commands().front();
            QCOMPARE(executed.kind, ssa::ports::ExternalCommandKind::OpenSsa);
            QCOMPARE(QString::fromStdString(executed.parameters.at("ssa_number")),
                     QString("202500001"));
            QTRY_COMPARE_WITH_TIMEOUT(model.lastMessage(), QString("ok"), 1000);
            QCOMPARE(model.lastStatus(), QString("succeeded"));
            QCOMPARE(model.lastSucceeded(), true);
        }

        void command_view_model_preserves_not_implemented_status() {
            auto commands = std::make_shared<FakeCommands>();
            commands->nextResult = {ssa::ports::ExternalCommandStatus::NotImplemented,
                                    "not implemented"};
            ssa::presentation::CommandViewModel model(commands);

            model.openSamHome();

            QTRY_COMPARE_WITH_TIMEOUT(model.lastStatus(), QString("not_implemented"), 1000);
            QCOMPARE(model.lastSucceeded(), false);
        }

        void command_view_model_exposes_configured_local_paths() {
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::CommandViewModel model(commands);

            model.openInputFolder();

            QTRY_COMPARE_WITH_TIMEOUT(commands->commands().size(), std::size_t{1}, 1000);
            QCOMPARE(commands->commands().front().kind,
                     ssa::ports::ExternalCommandKind::OpenInputFolder);
            QTRY_COMPARE_WITH_TIMEOUT(model.lastStatus(), QString("succeeded"), 1000);
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationSmokeTest)

#include "PresentationSmokeTest.moc"
