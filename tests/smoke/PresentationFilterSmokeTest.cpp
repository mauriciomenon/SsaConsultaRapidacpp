#include "PresentationSmokeFakes.h"

#include "presentation/AdvancedDerivationFilterViewModel.h"
#include "presentation/AdvancedTextFilterViewModel.h"
#include "presentation/AdvancedWeekFilterViewModel.h"
#include "presentation/FilterPanelAdvancedViewModel.h"
#include "presentation/MainViewModel.h"

#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QVariantMap>
#include <QtTest>

#include <memory>
#include <string>

namespace {

    using ssa::tests::presentation_smoke::activeFilterEntry;
    using ssa::tests::presentation_smoke::FakeCommands;
    using ssa::tests::presentation_smoke::FakeFilterPresetStore;
    using ssa::tests::presentation_smoke::FakePreferences;
    using ssa::tests::presentation_smoke::FakeRepository;

    class PresentationFilterSmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void column_filter_apply_signal_reloads_table_with_responsible_execution_filter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* columns = qobject_cast<ssa::presentation::ColumnFilterViewModel*>(
                model.browse()->filters()->columns());
            QVERIFY(columns != nullptr);

            columns->applyFilterFor("responsavel_execucao", "Ana");

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));
            QCOMPARE(QString::fromStdString(request.columnFilters.at("responsavel_execucao")),
                     QString("Ana"));
        }

        void active_filter_removal_reloads_table_without_removed_column_filter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->filters()->setColumnFilters({{"responsavel_execucao", "Ana"}});
            QCOMPARE(model.browse()->filters()->removeActiveFilter(QVariantMap{
                         {QStringLiteral("kind"), QStringLiteral("column")},
                         {QStringLiteral("key"), QStringLiteral("responsavel_execucao")}}),
                     true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QVERIFY(!repository->requests().back().columnFilters.contains("responsavel_execucao"));
        }

        void manual_filter_state_summary_and_request_stay_in_sync() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->setQuickSector("MEG2");
            model.browse()->filters()->setColumnFilters({{"responsavel_execucao", "Ana"}});
            text->setTextFilter("situacao", "=APV");

            QCOMPARE(repository->requests().size(), std::size_t{0});
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->filters()->activeFilterEntries().size(), 3,
                                      1000);
            QVERIFY(!activeFilterEntry(model.browse()->filters(), "quick_sector").isEmpty());
            QVERIFY(!activeFilterEntry(model.browse()->filters(), "column", "responsavel_execucao")
                         .isEmpty());
            QVERIFY(!activeFilterEntry(model.browse()->filters(), "advanced_text", "situacao")
                         .isEmpty());
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("MEG2"));
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("Resp. Exec"));
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("Sit"));
            QVERIFY(model.browse()->filters()->activeFilterSummary().contains("APV"));

            model.browse()->apply();

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto request = repository->requests().back();
            QCOMPARE(QString::fromStdString(request.quickSector), QString("MEG2"));
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));
            QCOMPARE(QString::fromStdString(request.columnFilters.at("responsavel_execucao")),
                     QString("Ana"));
            QVERIFY(request.advancedFilters.textFilters.contains("situacao"));
            QCOMPARE(QString::fromStdString(request.advancedFilters.textFilters.at("situacao")),
                     QString("=APV"));
        }

        void summary_removal_reloads_only_the_removed_filter_family() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->setQuickSector("MEG2");
            model.browse()->filters()->setColumnFilters({{"responsavel_execucao", "Ana"}});
            text->setTextFilter("situacao", "=APV");
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->filters()->activeFilterEntries().size(), 3,
                                      1000);

            const auto advancedEntry =
                activeFilterEntry(model.browse()->filters(), "advanced_text", "situacao");
            QVERIFY(!advancedEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(advancedEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            auto request = repository->requests().back();
            QVERIFY(!request.advancedFilters.textFilters.contains("situacao"));
            QCOMPARE(QString::fromStdString(request.quickSector), QString("MEG2"));
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));

            const auto quickSectorEntry =
                activeFilterEntry(model.browse()->filters(), "quick_sector");
            QVERIFY(!quickSectorEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(quickSectorEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            request = repository->requests().back();
            QCOMPARE(QString::fromStdString(request.quickSector), QString(""));
            QVERIFY(request.columnFilters.contains("responsavel_execucao"));
            QVERIFY(!request.advancedFilters.textFilters.contains("situacao"));
        }

        void exclusion_filter_setter_reloads_table() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);

            model.browse()->filters()->setExcludeScaSesSte(true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(repository->requests().back().excludeScaSesSte, true);
        }

        void advanced_text_filter_selection_reloads_table() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            QCOMPARE(text->updateFilterWithSelectedValue("responsavel_execucao", "Ana"), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            const auto textFilters = repository->requests().back().advancedFilters.textFilters;
            QVERIFY(textFilters.contains("responsavel_execucao"));
            QCOMPARE(QString::fromStdString(textFilters.at("responsavel_execucao")),
                     QString("=Ana"));
        }

        void advanced_text_filter_clear_reloads_table_without_filter() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setTextFilter("responsavel_execucao", "=Ana");
            QCOMPARE(text->clearTextFilterAndApply("responsavel_execucao"), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QVERIFY(!repository->requests().back().advancedFilters.textFilters.contains(
                "responsavel_execucao"));
        }

        void column_filter_summary_uses_contains_marker() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("situacao");
            filters.setColumnValue("APV");
            filters.addColumnFilter();

            QCOMPARE(filters.activeFilters().contains("Sit: APV"), true);
            QCOMPARE(filters.activeFilterSummary().contains("Sit: APV"), true);
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
            derivation->setReprogrammingMode("gte");
            derivation->setReprogrammingValues({"1", "3", "5"});
            week->setIssueWeekStartFilter("202501");
            week->setIssueWeekEndFilter("202510");
            week->setExecutionWeekStartFilter("202601");
            week->setExecutionWeekEndFilter("202620");
            derivation->setDerivationMode("derived");
            derivation->setOnlyReprogrammed(true);
            QCOMPARE(repository->requests().size(), std::size_t{0});
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
            QCOMPARE(request.advancedFilters.reprogrammingComparison,
                     ssa::domain::NumericComparisonMode::GreaterOrEqual);
            QVERIFY(request.advancedFilters.reprogrammingValues == (std::vector<int>{1, 3, 5}));
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

            text->replaceWithOperatorValueLists("situacao", {"SCA", "SES"}, {"STE"});

            QCOMPARE(text->textFilter("situacao"), QString("=SCA,=SES,!STE"));
            QCOMPARE(text->operatorModeFor("situacao"), QString("mixed"));
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

        void advanced_summary_removal_reloads_week_and_derivation_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* week =
                qobject_cast<ssa::presentation::AdvancedWeekFilterViewModel*>(advanced->week());
            auto* derivation = qobject_cast<ssa::presentation::AdvancedDerivationFilterViewModel*>(
                advanced->derivation());
            QVERIFY(week != nullptr);
            QVERIFY(derivation != nullptr);

            week->setIssueYearFilter("2026");
            derivation->setOnlyReprogrammed(true);
            QTRY_COMPARE_WITH_TIMEOUT(model.browse()->filters()->activeFilterEntries().size(), 3,
                                      1000);
            QVERIFY(!activeFilterEntry(model.browse()->filters(), "quick_sector").isEmpty());

            const auto issueYearEntry =
                activeFilterEntry(model.browse()->filters(), "advanced_issue_year");
            QVERIFY(!issueYearEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(issueYearEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            auto request = repository->requests().back();
            QVERIFY(!request.advancedFilters.issueYear.has_value());
            QCOMPARE(request.advancedFilters.onlyReprogrammed, true);

            const auto reprogrammedEntry =
                activeFilterEntry(model.browse()->filters(), "advanced_only_reprogrammed");
            QVERIFY(!reprogrammedEntry.isEmpty());
            QCOMPARE(model.browse()->filters()->removeActiveFilter(reprogrammedEntry), true);

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{2}, 1000);
            request = repository->requests().back();
            QVERIFY(!request.advancedFilters.issueYear.has_value());
            QCOMPARE(request.advancedFilters.onlyReprogrammed, false);
            const auto entries = model.browse()->filters()->activeFilterEntries();
            QCOMPARE(entries.size(), 1);
            QCOMPARE(entries.at(0).toMap().value("kind").toString(), QString("quick_sector"));
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
            QCOMPARE(qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector())
                         ->excludeScaSesSte(),
                     false);
        }

        void reset_filters_restores_default_sca_ses_ste_exclusion() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            ssa::presentation::FilterPanelViewModel filters(service);

            filters.setColumnKey("setor_executor");
            filters.resetFilters();

            QCOMPARE(filters.columnKey(), QString("situacao"));
            QCOMPARE(qobject_cast<ssa::presentation::FilterPanelSectorViewModel*>(filters.sector())
                         ->excludeScaSesSte(),
                     false);
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

        void status_shortcuts_toggle_advanced_status_filter_and_apply() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->toggleStatusShortcut("APV");

            QCOMPARE(text->textFilter("situacao"), QString("=APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(QString::fromStdString(
                         repository->requests().back().advancedFilters.textFilters.at("situacao")),
                     QString("=APV"));

            model.browse()->filters()->toggleStatusShortcut("STE");

            QCOMPARE(text->textFilter("situacao"), QString("=APV,=STE"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("STE"));

            model.browse()->filters()->toggleStatusShortcut("APV");

            QCOMPARE(text->textFilter("situacao"), QString("=STE"));
            QVERIFY(!model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("STE"));
        }

        void status_shortcuts_do_not_mark_excluded_status_values() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::presentation::MainViewModel model(service, commands);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            text->setTextFilter("situacao", "!STE");

            QVERIFY(!model.browse()->filters()->statusShortcutSelected("STE"));

            text->setTextFilter("situacao", "=APV,!STE");

            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(!model.browse()->filters()->statusShortcutSelected("STE"));

            model.browse()->filters()->toggleStatusShortcut("STE");

            QCOMPARE(text->textFilter("situacao"), QString("=APV,=STE"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("APV"));
            QVERIFY(model.browse()->filters()->statusShortcutSelected("STE"));
        }

        void named_saved_filter_persists_applies_and_removes_current_filters() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->search()->setText("bomba");
            model.browse()->filters()->setQuickSector("MEG2");
            QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                      Q_ARG(QString, QString("braba")));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(preferences->snapshot().savedFilters.size(), std::size_t{1});
            QCOMPARE(QString::fromStdString(preferences->snapshot().savedFilters.front().name),
                     QString("braba"));
            QCOMPARE(QString::fromStdString(
                         preferences->snapshot().savedFilters.front().filters.searchText),
                     QString("bomba"));

            model.browse()->search()->setText("outra");
            model.browse()->filters()->setQuickSector("MMU3");
            QMetaObject::invokeMethod(model.preferenceFlow(), "applySavedFilter",
                                      Q_ARG(QString, QString("braba")));

            QTRY_COMPARE_WITH_TIMEOUT(repository->requests().size(), std::size_t{1}, 1000);
            QCOMPARE(model.browse()->search()->text(), QString("bomba"));
            QCOMPARE(model.browse()->filters()->quickSector(), QString("MEG2"));

            QMetaObject::invokeMethod(model.preferenceFlow(), "removeSavedFilter",
                                      Q_ARG(QString, QString("braba")));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->snapshot().savedFilters.size(), std::size_t{0},
                                      1000);
        }

        void named_saved_filter_normalizes_overlapping_filter_families() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            auto preferences = std::make_shared<FakePreferences>();
            ssa::presentation::MainViewModel model(service, commands, preferences);
            auto* advanced = qobject_cast<ssa::presentation::FilterPanelAdvancedViewModel*>(
                model.browse()->filters()->advanced());
            QVERIFY(advanced != nullptr);
            auto* text =
                qobject_cast<ssa::presentation::AdvancedTextFilterViewModel*>(advanced->text());
            QVERIFY(text != nullptr);

            model.browse()->filters()->setQuickSector("IEE3");
            model.browse()->filters()->setColumnFilters({{"situacao", "=APV"}});
            text->setTextFilter("setor_executor", "=IEE1");
            text->setTextFilter("situacao", "=SCA");
            model.browse()->filters()->setExcludeScaSesSte(true);
            QMetaObject::invokeMethod(model.preferenceFlow(), "saveCurrentFilter",
                                      Q_ARG(QString, QString("normalizado")));

            QTRY_COMPARE_WITH_TIMEOUT(preferences->saveCount(), 1, 1000);
            QCOMPARE(preferences->snapshot().savedFilters.size(), std::size_t{1});
            const auto saved = preferences->snapshot().savedFilters.front().filters;
            QVERIFY(saved.quickSector.empty());
            QVERIFY(saved.columnFilters.empty());
            QVERIFY(!saved.excludeScaSesSte);
            QCOMPARE(QString::fromStdString(saved.advancedTextFilters.at("setor_executor")),
                     QString("=IEE1,=IEE3"));
            QCOMPARE(QString::fromStdString(saved.advancedTextFilters.at("situacao")),
                     QString("=SCA"));
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

        void filter_summary_removal_preserves_sort_and_visible_columns() {
            auto repository = std::make_shared<FakeRepository>();
            auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
            auto commands = std::make_shared<FakeCommands>();
            ssa::ports::UserPreferencesSnapshot initial;
            initial.visibleColumns = {"numero_ssa", "situacao"};
            initial.columnWidths = {{"numero_ssa", 160}, {"situacao", 140}};
            initial.sortColumnKey = "situacao";
            initial.sortAscending = true;
            auto preferences = std::make_shared<FakePreferences>(initial);
            ssa::presentation::MainViewModel model(service, commands, preferences);

            model.browse()->filters()->setColumnFilters({{"situacao", "=APV"}});

            QCOMPARE(model.browse()->filters()->removeActiveFilter(
                         QVariantMap{{QStringLiteral("kind"), QStringLiteral("column")},
                                     {QStringLiteral("key"), QStringLiteral("situacao")}}),
                     true);

            QVERIFY(model.browse()->filters()->columnFilters().empty());
            QCOMPARE(model.browse()->sortColumnKey(), QString("situacao"));
            QCOMPARE(model.browse()->sortAscending(), true);
            const auto visibleColumns = model.browse()->visibleColumns();
            QCOMPARE(visibleColumns.size(), std::size_t{2});
            QCOMPARE(QString::fromStdString(visibleColumns.at(0)), QString("numero_ssa"));
            QCOMPARE(QString::fromStdString(visibleColumns.at(1)), QString("situacao"));
        }
    };

} // namespace

QTEST_GUILESS_MAIN(PresentationFilterSmokeTest)

#include "PresentationFilterSmokeTest.moc"
