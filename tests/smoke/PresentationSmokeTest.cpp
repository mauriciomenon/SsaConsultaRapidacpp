#include "PresentationSmokeFakes.h"

#include "application/SsaWorkflowService.h"
#include "domain/SsaTypes.h"
#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/FilterPanelViewModel.h"
#include "presentation/MainViewModel.h"
#include "presentation/SsaRecordValueFormatter.h"

#include <QChar>
#include <QObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtTest>

#include <memory>
#include <string>

namespace {

    using ssa::tests::presentation_smoke::CapturingDerivadasPort;
    using ssa::tests::presentation_smoke::CapturingImportPort;
    using ssa::tests::presentation_smoke::FakeCommands;
    using ssa::tests::presentation_smoke::FakePreferences;
    using ssa::tests::presentation_smoke::FakeRepository;
    using ssa::tests::presentation_smoke::FakeRepositoryConfig;

    class PresentationSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void date_text_formatter_keeps_only_day_month_year() {
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "2026-04-13 11:21:01", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "2026-04-13T12:26:00", ssa::domain::ColumnType::DateText)
                         .toString(),
                     QString("13/04/2026"));
            QCOMPARE(ssa::presentation::SsaRecordValueFormatter::valueFor(
                         "sem data", ssa::domain::ColumnType::DateText)
                         .toString(),
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

        void search_apply_signal_reloads_table() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->search()->setText("bomba");
            model.browse()->search()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(QString::fromStdString(repository->requests().back().searchText),
                     QString("bomba"));
        }

        void details_navigation_walks_next_then_prev_within_page() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{3}, .rowCount = std::size_t{3}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QSignalSpy rowSpy(model.browse(),
                              &ssa::presentation::BrowseViewModel::currentRowChanged);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 3, 1000);

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));
            QVERIFY(model.browse()->canSelectNextRow());
            QVERIFY(!model.browse()->canSelectPreviousRow());
            QCOMPARE(rowSpy.count(), 1);

            model.browse()->selectNextRow();

            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 1, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500002"));
            QVERIFY(model.browse()->canSelectNextRow());
            QVERIFY(model.browse()->canSelectPreviousRow());

            model.browse()->selectNextRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 2, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500003"));
            QVERIFY(!model.browse()->canSelectNextRow());
            QVERIFY(model.browse()->canSelectPreviousRow());

            model.browse()->selectNextRow();
            QVERIFY(model.browse()->currentRow() == 2);

            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 1, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500002"));

            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));

            model.browse()->selectPreviousRow();
            QVERIFY(model.browse()->currentRow() == 0);
        }

        void select_by_ssa_number_picks_row_when_present_on_page() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{3}, .rowCount = std::size_t{3}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 3, 1000);

            QVERIFY(model.browse()->selectRowBySsaNumber(QString("202500002")));
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 1, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500002"));
        }

        void select_by_ssa_number_falls_back_to_search_when_absent() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{3}, .rowCount = std::size_t{3}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 3, 1000);

            QVERIFY(model.browse()->selectRowBySsaNumber(QString("202599999")));
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->search()->text(), QString("202599999"), 1000);
        }

        void select_by_ssa_number_rejects_empty_input() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            QVERIFY(!model.browse()->selectRowBySsaNumber(QString("")));
            QVERIFY(!model.browse()->selectRowBySsaNumber(QString("   ")));
        }

        void details_navigation_walks_across_pages_next_then_prev() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{25}, .rowCount = std::size_t{10}});
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->setPageSize(10);
            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 10, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->pageCount(), 3);

            // Walk to last row of page 1
            for (int i = 0; i < 9; ++i) {
                model.browse()->selectNextRow();
            }
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 9, 1000);
            QVERIFY(model.browse()->canSelectNextRow());
            QCOMPARE(model.browse()->pageNumber(), 1);

            // Cross to page 2 -> auto-select first row
            model.browse()->selectNextRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 0, 1000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500001"));

            // Previous from first row of page 2 -> back to page 1 last row
            QVERIFY(model.browse()->canSelectPreviousRow());
            model.browse()->selectPreviousRow();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 1, 2000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->currentRow(), 9, 2000);
            QCOMPARE(model.browse()->details()->selectedSsa(), QString("202500010"));
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

        void sort_cycle_ascends_descends_then_clears() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), false);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString(""));
            QCOMPARE(model.browse()->sortAscending(), false);

            model.browse()->sortByColumn(1);
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);
        }

        void sort_by_column_resets_page_and_saves_preferences() {
            const auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{21}});
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->setPageSize(10);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageCount(), 3, 1000);
            model.browse()->nextPage();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 2, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().back().pageIndex, std::size_t{1},
                                      1000);

            model.browse()->sortByColumn(1);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().back().pageIndex, std::size_t{0},
                                      1000);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->pageNumber(), 1, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(QString::fromStdString(preferences->snapshot().sortColumnKey),
                                      QString("situacao"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().sortAscending, true, 1000);
        }

        void table_headers_expose_sort_indicator_state() {
            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            model.browse()->sortByColumn(1);

            const auto headers = model.browse()->tableHeaders();
            QVERIFY(headers.size() > 1);
            const auto sortedHeader = headers[1].toMap();
            QCOMPARE(sortedHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(sortedHeader.value("sorted").toBool(), true);
            QCOMPARE(sortedHeader.value("sortAscending").toBool(), true);

            const auto unsortedHeader = headers[0].toMap();
            QCOMPARE(unsortedHeader.value("sorted").toBool(), false);

            model.browse()->sortByColumn(1);
            const auto descendingHeaders = model.browse()->tableHeaders();
            const auto descendingHeader = descendingHeaders[1].toMap();
            QCOMPARE(descendingHeader.value("key").toString(), QString("situacao"));
            QCOMPARE(descendingHeader.value("sorted").toBool(), true);
            QCOMPARE(descendingHeader.value("sortAscending").toBool(), false);
        }

        void next_page_reaches_final_page() {
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.totalRows = std::size_t{21}});
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
            auto repository = std::make_shared<FakeRepository>(
                FakeRepositoryConfig{.delay = std::chrono::milliseconds{80}});
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

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 180));
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

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 220);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 220,
                                      1000);
        }

        void apply_column_settings_persists_applied_snapshot_not_later_staging() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
            QMetaObject::invokeMethod(model.columnFlow(), "applyColumnSettings");

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 300));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{2},
                                      1000);
            const auto saved = preferences->snapshot();
            QCOMPARE(QString::fromStdString(saved.visibleColumns.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(saved.visibleColumns.at(1)), QString("situacao"));
            QCOMPARE(saved.columnWidths.at("numero_ssa"), 220);
        }

        void column_width_flow_persists_without_reloading_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnWidthAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(int, 230));

            QCOMPARE(changed, true);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QCOMPARE(model.browse()->tableModel()->columnWidth(0), 230);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 230,
                                      1000);
        }

        void immediate_column_width_flow_ignores_unapplied_popup_edits() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 230));
            QVERIFY(model.columns()->setColumnWidth("situacao", 260));

            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnWidthAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(int, 230));

            QCOMPARE(changed, true);
            QCOMPARE(repository->requests().size(), std::size_t{1});
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 230,
                                      1000);
            QCOMPARE(preferences->snapshot().columnWidths.at("situacao"), 160);
            QCOMPARE(preferences->snapshot().visibleColumns.size(), std::size_t{3});
            QCOMPARE(QString::fromStdString(preferences->snapshot().visibleColumns.at(2)),
                     QString("setor_executor"));
        }

        void column_visibility_flow_persists_and_reloads_query() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 140}, {"situacao", 160}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "situacao"),
                                      Q_ARG(bool, false));

            QCOMPARE(changed, true);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QCOMPARE(repository->requests().back().visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(repository->requests().back().visibleColumns.front()),
                     QString("numero_ssa"));
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{1},
                                      1000);
        }

        void immediate_column_visibility_flow_ignores_unapplied_popup_edits() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao", "setor_executor"};
            initial.columnWidths = {
                {"numero_ssa", 140}, {"situacao", 160}, {"setor_executor", 180}};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->load();
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->tableModel()->rowCount(), 1, 1000);
            QVERIFY(model.columns()->setColumnVisibleByKey("setor_executor", false));
            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 230));

            bool canHideStagedColumn = false;
            QMetaObject::invokeMethod(model.columnFlow(), "canHideColumn",
                                      qReturnArg(canHideStagedColumn),
                                      Q_ARG(QString, "setor_executor"));
            bool changed = false;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "situacao"),
                                      Q_ARG(bool, false));

            QCOMPARE(canHideStagedColumn, true);
            QCOMPARE(changed, true);
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.size(), std::size_t{2},
                                      1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                QString::fromStdString(preferences->snapshot().visibleColumns.at(0)),
                QString("numero_ssa"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(
                QString::fromStdString(preferences->snapshot().visibleColumns.at(1)),
                QString("setor_executor"), 1000);
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().columnWidths.at("numero_ssa"), 140,
                                      1000);
            const auto stagedVisibleKeys = model.columns()->visibleKeys();
            QCOMPARE(stagedVisibleKeys.size(), std::size_t{2});
            QCOMPARE(QString::fromStdString(stagedVisibleKeys.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(stagedVisibleKeys.at(1)), QString("setor_executor"));
        }

        void column_visibility_flow_keeps_one_visible_column() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa"};

            const auto repository = std::make_shared<FakeRepository>();
            const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            const auto commands = std::make_shared<FakeCommands>();
            const auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            bool canHide = true;
            QMetaObject::invokeMethod(model.columnFlow(), "canHideColumn", qReturnArg(canHide),
                                      Q_ARG(QString, "numero_ssa"));
            bool changed = true;
            QMetaObject::invokeMethod(model.columnFlow(), "setColumnVisibleAndApply",
                                      qReturnArg(changed), Q_ARG(QString, "numero_ssa"),
                                      Q_ARG(bool, false));

            QCOMPARE(canHide, false);
            QCOMPARE(changed, false);
            QCOMPARE(model.columns()->visibleKeys().size(), std::size_t{1});
            QCOMPARE(repository->requests().size(), std::size_t{0});
            QCOMPARE(preferences->snapshot().visibleColumns.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(preferences->snapshot().visibleColumns.front()),
                     QString("numero_ssa"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void theme_preference_is_saved() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.ui()->setTheme("tokyo-night");

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(model.ui()->theme(), QString("tokyo-night"));
            QCOMPARE(QString::fromStdString(preferences->snapshot().theme), QString("tokyo-night"));
        }

        void pyqt_theme_catalog_is_accepted() {
            const QStringList themes{
                "grayscale",  "windows7", "classico",       "gruvbox",
                "dark",       "dracula",  "solarized-dark", "solarized-light",
                "mint-light", "paper",    "tokyo-night",    "catppuccin",
                "nord",
            };

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            for (const QString& theme : themes) {
                model.ui()->setTheme(theme);
                QCOMPARE(model.ui()->theme(), theme);
            }
        }

        void theme_can_be_reverted_after_preview_change() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            const QString original = model.ui()->theme();
            QVERIFY(!original.isEmpty());

            QSignalSpy themeSpy(model.ui(), &ssa::presentation::UiSettingsViewModel::themeChanged);
            model.ui()->setTheme("dark");
            QCOMPARE(model.ui()->theme(), QString("dark"));
            QVERIFY(themeSpy.count() >= 1);

            // Simulate ThemeDialog::reject() restoring the original theme
            themeSpy.clear();
            model.ui()->setTheme(original);
            QCOMPARE(model.ui()->theme(), original);
            QVERIFY(themeSpy.count() >= 1);
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

            QCOMPARE(model.actions()->currentWeek()->value(),
                     model.actions()->currentWeek()->label());
            QVERIFY(
                model.actions()->currentWeek()->value().contains(QRegularExpression("^\\d{6}$")));
        }

        void preferences_save_failure_reports_store_error() {
            auto preferences = std::make_shared<FakePreferences>();
            preferences->failNextSave("disk full");
            ssa::presentation::UserPreferencesCoordinator coordinator(preferences);
            QSignalSpy failedSpy(&coordinator,
                                 &ssa::presentation::UserPreferencesCoordinator::saveFailed);

            coordinator.saveNowOrSchedule({});

            QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 1000);
            const auto message = failedSpy.takeFirst().at(0).toString();
            QVERIFY(message.contains("disk full"));
            QCOMPARE(preferences->saveCount(), 0);
        }

        void preferences_save_rejects_wrong_thread_call_without_saving() {
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::UserPreferencesCoordinator coordinator(preferences);
            QSignalSpy failedSpy(&coordinator,
                                 &ssa::presentation::UserPreferencesCoordinator::saveFailed);

            std::thread worker([&coordinator] { coordinator.saveNowOrSchedule({}); });
            worker.join();

            QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 1000);
            const auto message = failedSpy.takeFirst().at(0).toString();
            QVERIFY(message.contains("thread"));
            QCOMPARE(preferences->saveCount(), 0);
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

            QVERIFY(model.columns()->setColumnVisibleByKey("situacao", false));
            QVERIFY(model.columns()->setColumnWidth("numero_ssa", 220));
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

        void column_move_reorders_visible_keys() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            // Find the first two visible columns to swap.
            const auto keys = model.columns()->visibleKeys();
            QVERIFY(keys.size() >= 2);
            const auto keyA = QString::fromStdString(keys[0]);
            const auto keyB = QString::fromStdString(keys[1]);

            // Find the model rows for these two keys in a single pass.
            int rowA = -1;
            int rowB = -1;
            for (int i = 0; i < model.columns()->rowCount(); ++i) {
                const auto key = model.columns()
                                     ->data(model.columns()->index(i),
                                            ssa::presentation::ColumnSettingsModel::KeyRole)
                                     .toString();
                if (key == keyA) {
                    rowA = i;
                }
                if (key == keyB) {
                    rowB = i;
                }
            }
            QVERIFY(rowA >= 0);
            QVERIFY(rowB >= 0);

            const auto allKeys = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(allKeys[0]), keyA);
            QCOMPARE(QString::fromStdString(allKeys[1]), keyB);

            const bool moved = model.columns()->moveColumn(rowA, rowB);
            QCOMPARE(moved, true);

            const auto keysAfter = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(keysAfter[0]), keyB);
            QCOMPARE(QString::fromStdString(keysAfter[1]), keyA);
        }

        void column_move_persists_order_through_preferences() {
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"situacao", "numero_ssa"};

            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            const auto initialKeys = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(initialKeys[0]), QString("situacao"));
            QCOMPARE(QString::fromStdString(initialKeys[1]), QString("numero_ssa"));

            // Move column 0 (situacao) to position 1, swapping the visible order.
            int rowA = -1;
            int rowB = -1;
            for (int i = 0; i < model.columns()->rowCount(); ++i) {
                const auto key = model.columns()
                                     ->data(model.columns()->index(i),
                                            ssa::presentation::ColumnSettingsModel::KeyRole)
                                     .toString();
                if (key == QString("situacao")) {
                    rowA = i;
                }
                if (key == QString("numero_ssa")) {
                    rowB = i;
                }
            }
            QVERIFY(rowA >= 0);
            QVERIFY(rowB >= 0);

            const bool moved = model.columns()->moveColumn(rowA, rowB);
            QCOMPARE(moved, true);
            qobject_cast<ssa::presentation::MainColumnFlowCoordinator*>(model.columnFlow())
                ->applyColumnSettings();

            // Verify the in-memory order flipped.
            const auto reordered = model.columns()->visibleKeys();
            QCOMPARE(QString::fromStdString(reordered[0]), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(reordered[1]), QString("situacao"));

            // Verify the new order actually persisted into the preferences store.
            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().visibleColumns.front(),
                                      std::string("numero_ssa"), 1000);
            QCOMPARE(preferences->snapshot().visibleColumns.at(1), std::string("situacao"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationSmokeTest)

#include "PresentationSmokeTest.moc"
