#include "application/SsaWorkflowService.h"
#include "domain/SsaTypes.h"
#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/MainViewModel.h"
#include "presentation/SsaRecordValueFormatter.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
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

    class FakeFilterPresetStore final : public ssa::ports::IFilterPresetStore {
      public:
        ssa::ports::FilterPresetSnapshot load(std::filesystem::path) const override {
            const std::scoped_lock lock(mutex_);
            return nextLoad_;
        }

        void save(std::filesystem::path path,
                  const ssa::ports::FilterPresetSnapshot& snapshot) const override {
            const std::scoped_lock lock(mutex_);
            savedPath_ = std::move(path);
            saved_ = snapshot;
            ++saveCount_;
        }

        void setNextLoad(ssa::ports::FilterPresetSnapshot snapshot) {
            const std::scoped_lock lock(mutex_);
            nextLoad_ = std::move(snapshot);
        }

        [[nodiscard]] int saveCount() const {
            const std::scoped_lock lock(mutex_);
            return saveCount_;
        }

        [[nodiscard]] ssa::ports::FilterPresetSnapshot saved() const {
            const std::scoped_lock lock(mutex_);
            return saved_;
        }

      private:
        mutable std::mutex mutex_;
        mutable ssa::ports::FilterPresetSnapshot saved_;
        mutable std::filesystem::path savedPath_;
        mutable int saveCount_{0};
        ssa::ports::FilterPresetSnapshot nextLoad_;
    };

    class CapturingImportPort final : public ssa::ports::IImportWorkflowPort {
      public:
        ssa::ports::WorkflowResult
        importExternalFiles(const ssa::ports::ImportExternalFilesRequest& request) override {
            const std::scoped_lock lock(mutex_);
            importRequests_.push_back(request);
            return {ssa::ports::WorkflowStatus::Succeeded, "import staged"};
        }

        ssa::ports::WorkflowResult rescan(const ssa::ports::RescanRequest& request) override {
            const std::scoped_lock lock(mutex_);
            requests_.push_back(request);
            return {ssa::ports::WorkflowStatus::Succeeded, "rescan requested"};
        }

        [[nodiscard]] std::vector<ssa::ports::RescanRequest> requests() const {
            const std::scoped_lock lock(mutex_);
            return requests_;
        }

        [[nodiscard]] std::vector<ssa::ports::ImportExternalFilesRequest> importRequests() const {
            const std::scoped_lock lock(mutex_);
            return importRequests_;
        }

      private:
        mutable std::mutex mutex_;
        std::vector<ssa::ports::ImportExternalFilesRequest> importRequests_;
        std::vector<ssa::ports::RescanRequest> requests_;
    };

    class CapturingDerivadasPort final : public ssa::ports::IDerivadasPort {
      public:
        explicit CapturingDerivadasPort(
            ssa::ports::WorkflowResult result = {ssa::ports::WorkflowStatus::Succeeded,
                                                 "derivadas sync completed"})
            : nextResult_(std::move(result)) {}

        ssa::ports::WorkflowResult syncDerivadas() override {
            const std::scoped_lock lock(mutex_);
            ++syncCalls_;
            return nextResult_;
        }

        [[nodiscard]] std::size_t syncCalls() const {
            const std::scoped_lock lock(mutex_);
            return syncCalls_;
        }

        void setNextResult(ssa::ports::WorkflowResult result) {
            const std::scoped_lock lock(mutex_);
            nextResult_ = std::move(result);
        }

      private:
        mutable std::mutex mutex_;
        std::size_t syncCalls_{0};
        ssa::ports::WorkflowResult nextResult_;
    };

    class PresentationSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void date_text_formatter_keeps_only_day_month_year() {
            const ssa::presentation::SsaRecordValueFormatter formatter;

            QCOMPARE(formatter.valueFor("2026-04-13 11:21:01", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(formatter.valueFor("2026-04-13T12:26:00", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(formatter.valueFor("sem data", ssa::domain::ColumnType::DateText).toString(),
                     QString("sem data"));
        }

        void load_populates_table_and_allows_details_selection() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            QSignalSpy pageSpy(model.browse(), &ssa::presentation::BrowseViewModel::pageChanged);

            model.browse()->search()->setText("Teste");
            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QCOMPARE(model.browse()->totalRows(), 1);
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
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

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
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

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
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(text != nullptr);
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);

            week->setWeekColumnKey("semana_programada");
            week->setYearFilter("2025");
            week->setWeekFilter("2");
            text->setTextFilter("situacao", "=APV");
            text->setTextFilter("setor_executor", "=SMM");
            week->setIssueYearFilter("2025");
            week->setExecutionYearFilter("2026");
            derivation->setReprogrammingEqualsFilter("1");
            week->setIssueWeekStartFilter("202501");
            week->setIssueWeekEndFilter("202510");
            week->setExecutionWeekStartFilter("202601");
            week->setExecutionWeekEndFilter("202620");
            derivation->setDerivationMode("derived");
            derivation->setOnlyReprogrammed(true);
            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(QString::fromStdString(request.advancedFilters.weekColumnKey),
                     QString("semana_programada"));
            QCOMPARE(request.advancedFilters.year.value_or(0), 2025);
            QCOMPARE(request.advancedFilters.week.value_or(0), 2);
            QVERIFY(request.advancedFilters.textFilters.at("situacao") == "=APV");
            QVERIFY(request.advancedFilters.textFilters.at("setor_executor") == "=SMM");
            QCOMPARE(request.advancedFilters.issueYear.value_or(0), 2025);
            QCOMPARE(request.advancedFilters.executionYear.value_or(0), 2026);
            QCOMPARE(request.advancedFilters.reprogrammingEquals.value_or(0), 1);
            QCOMPARE(request.advancedFilters.issueWeekStart.value_or(0), 202501);
            QCOMPARE(request.advancedFilters.issueWeekEnd.value_or(0), 202510);
            QCOMPARE(request.advancedFilters.executionWeekStart.value_or(0), 202601);
            QCOMPARE(request.advancedFilters.executionWeekEnd.value_or(0), 202620);
            QCOMPARE(request.advancedFilters.derivationMode,
                     ssa::domain::DerivationFilterMode::DerivedOnly);
            QCOMPARE(request.advancedFilters.onlyReprogrammed, true);
        }

        void advanced_text_filter_selection_builds_multi_value_terms() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            QCOMPARE(text->updateFilterWithSelectedValue("situacao", "APV"), true);
            text->setOperatorMode("situacao", "different");
            QCOMPARE(text->updateFilterWithSelectedValue("situacao", "ADM"), true);
            QCOMPARE(text->updateFilterWithSelectedValue("situacao", "APV"), false);

            QCOMPARE(text->textFilter("situacao"), QString("=APV,!ADM"));
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));

            text->setOperatorMode("situacao", "different");
            QCOMPARE(text->operatorModeFor("situacao"), QString("different"));

            text->setOperatorMode("situacao", "mixed");
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));

            text->replaceWithOperatorValueList("situacao", {"APV", "ADM"}, "different");

            QCOMPARE(text->textFilter("situacao"), QString("!APV,!ADM"));
        }

        void advanced_text_filter_clear_preserves_selected_operator() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setOperatorMode("situacao", "different");
            text->updateFilterWithSelectedValue("situacao", "ASE");
            text->setTextFilter("situacao", "");

            QCOMPARE(text->textFilter("situacao"), QString(""));
            QCOMPARE(text->operatorModeFor("situacao"), QString("different"));
        }

        void advanced_submodels_update_shared_filter_state() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(text != nullptr);
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);

            text->updateFilterWithSelectedValue("situacao", "ASE");
            week->setIssueWeekStartFilter("202601");
            derivation->setDerivationMode("derived");

            QCOMPARE(text->textFilter("situacao"), QString("=ASE"));
            QCOMPARE(week->issueWeekStartFilter(), QString("202601"));
            QCOMPARE(derivation->derivationMode(), QString("derived"));
        }

        void clear_advanced_filters_keeps_column_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* advanced =
                qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(filters.advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(text != nullptr);
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);
            auto* columns =
                qobject_cast<ssa::presentation::ColumnFilterViewModel*>(filters.columns());
            QVERIFY(columns != nullptr);

            columns->applyFilterFor("descricao_ssa", "bomba");
            filters.setQuickSector("MEG2");
            text->setTextFilter("situacao", "=APV");
            week->setIssueYearFilter("2025");
            derivation->setOnlyReprogrammed(true);
            advanced->clear();

            QVERIFY(filters.columnFilters().at("descricao_ssa") == "bomba");
            QCOMPARE(text->textFilter("situacao"), QString(""));
            QCOMPARE(week->issueYearFilter(), QString(""));
            QCOMPARE(derivation->onlyReprogrammed(), false);
            QCOMPARE(filters.quickSector(), QString("MEG2"));
            QCOMPARE(filters.excludeScaSesSte(), true);
        }

        void reset_filters_restores_default_sca_ses_ste_exclusion() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("setor_executor");
            filters.resetFilters();

            QCOMPARE(filters.columnKey(), QString("situacao"));
            QCOMPARE(filters.excludeScaSesSte(), true);
            QCOMPARE(filters.activeFilters().contains("sem SCA/SES/STE"), false);
        }

        void column_filter_rows_publish_active_values() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);
            auto* columns =
                qobject_cast<ssa::presentation::ColumnFilterViewModel*>(filters.columns());
            QVERIFY(columns != nullptr);

            columns->applyFilterFor("descricao_ssa", "bomba");

            QString publishedValue;
            for (const auto& row : columns->rows()) {
                const auto map = row.toMap();
                if (map.value("key").toString() == "descricao_ssa") {
                    publishedValue = map.value("value").toString();
                    break;
                }
            }

            QCOMPARE(publishedValue, QString("bomba"));
            filters.resetFilters();

            QString resetValue;
            for (const auto& row : columns->rows()) {
                const auto map = row.toMap();
                if (map.value("key").toString() == "descricao_ssa") {
                    resetValue = map.value("value").toString();
                    break;
                }
            }
            QCOMPARE(resetValue, QString(""));
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

        void current_week_exposes_iso_label_for_header() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QVERIFY(
                model.actions()->currentWeek()->value().contains(QRegularExpression("^20\\d{4}$")));
            QVERIFY(model.actions()->currentWeek()->label().startsWith("Semana Atual: 20"));
        }

        void explicit_save_filters_button_persists_current_snapshot() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->filters()->setQuickSector("MEG2");
            QMetaObject::invokeMethod(model.preferenceFlow(), "savePreferences");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(QString::fromStdString(preferences->snapshot().filters.quickSector),
                     QString("MEG2"));
        }

        void filter_preset_export_uses_current_filters_without_search_text() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            auto presets = std::make_shared<FakeFilterPresetStore>();
            ssa::presentation::MainViewModel model(service, commands, preferences, presets);

            model.browse()->search()->setText("nao exportar");
            model.browse()->filters()->setQuickSector("MEG2");
            QMetaObject::invokeMethod(
                model.preferenceFlow(), "exportFilterPreset",
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json")));

            QTRY_COMPARE_WITH_TIMEOUT(presets->saveCount(), 1, 1000);
            QCOMPARE(QString::fromStdString(presets->saved().filters.quickSector), QString("MEG2"));
            QCOMPARE(QString::fromStdString(presets->saved().filters.searchText), QString(""));
        }

        void filter_preset_import_preserves_search_text_and_applies_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            auto presets = std::make_shared<FakeFilterPresetStore>();
            ssa::ports::FilterPresetSnapshot preset;
            preset.filters.quickSector = "MMU3";
            preset.filters.columnFilters = {{"situacao", "=APV"}};
            presets->setNextLoad(preset);
            ssa::presentation::MainViewModel model(service, commands, preferences, presets);

            model.browse()->search()->setText("manter busca");
            QMetaObject::invokeMethod(
                model.preferenceFlow(), "importFilterPreset",
                Q_ARG(QUrl, QUrl::fromLocalFile("/tmp/ssa-filter-preset.json")));

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(model.browse()->search()->text(), QString("manter busca"));
            QCOMPARE(model.browse()->filters()->quickSector(), QString("MMU3"));
            QCOMPARE(model.browse()->filters()->columnFilters().at("situacao"),
                     std::string("=APV"));
        }

        void rescan_incremental_uses_workflow_port_and_updates_status() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->rescanIncremental();

            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->requests().back().mode, ssa::ports::RescanMode::Incremental);
            QCOMPARE(importPort->requests().back().optimized, true);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Reescaneamento concluido"), 1000);
        }

        void import_external_files_uses_workflow_port_and_updates_status() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl::fromLocalFile("/tmp/entrada.xlsx"));
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->importRequests().back().files.size(), std::size_t{1});
            QCOMPARE(importPort->importRequests().back().optimized, true);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Importacao concluida"), 1000);
        }

        void import_external_files_rejects_non_local_url() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            selectedFiles.push_back(QUrl("https://example.com/entrada.xlsx"));
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{0}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao importar arquivos"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("Falha ao importar arquivos: apenas arquivos locais sao suportados"), 1000);
        }

        void import_external_files_rejects_empty_selection() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            QVariantList selectedFiles;
            model.actions()->workflows()->importExternalFiles(selectedFiles);

            QTRY_COMPARE_WITH_TIMEOUT(importPort->importRequests().size(), std::size_t{0}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao importar arquivos"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                model.browse()->status()->error(),
                QString("Falha ao importar arquivos: nenhum arquivo selecionado"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void sync_derivadas_updates_status_after_success() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->syncDerivadas();

            QTRY_COMPARE_WITH_TIMEOUT(derivadasPort->syncCalls(), std::size_t{1}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Sincronizacao de derivadas concluida"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), true);
        }

        void sync_derivadas_reports_workflow_error() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto derivadasPort = std::make_shared<CapturingDerivadasPort>(
                ssa::ports::WorkflowResult{ssa::ports::WorkflowStatus::Failed,
                                           "sync derivadas failed in integration path"});
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
                std::make_shared<CapturingImportPort>(), nullptr, nullptr, derivadasPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->syncDerivadas();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao sincronizar derivadas"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->error(),
                                      QString("sync derivadas failed in integration path"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void sync_derivadas_reports_not_configured_adapter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, nullptr);

            model.actions()->workflows()->syncDerivadas();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->message(),
                                      QString("Falha ao sincronizar derivadas"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->status()->error(),
                                      QString("sync derivadas workflow is not configured"), 1000);
            QCOMPARE(model.actions()->workflows()->lastSucceeded(), false);
        }

        void rescan_full_disables_optimized_mode() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto importPort = std::make_shared<CapturingImportPort>();
            auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(importPort);
            ssa::presentation::MainViewModel model(service, commands, nullptr, nullptr, workflows);

            model.actions()->workflows()->rescanFull();

            QTRY_COMPARE_WITH_TIMEOUT(importPort->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(importPort->requests().back().mode, ssa::ports::RescanMode::Full);
            QCOMPARE(importPort->requests().back().optimized, false);
        }

        void invalid_density_is_ignored() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setDensity("wide");

            QCOMPARE(model.ui()->density(), QString("compact"));
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
            QMetaObject::invokeMethod(model.columnFlow(), "discardColumnSettings");
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

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
