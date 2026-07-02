#include "domain/ColumnCatalog.h"
#include "infra/preferences/JsonFilterPresetStore.h"
#include "infra/preferences/JsonUserPreferencesStore.h"

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <filesystem>

TEST_CASE("json preferences store saves user preference snapshot") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    ssa::ports::UserPreferencesSnapshot snapshot;
    snapshot.pageSize = 50;
    snapshot.theme = "tokyo-night";
    snapshot.density = "compact";
    snapshot.detailsVisible = false;
    snapshot.detailsPanelWidth = 520;
    snapshot.sortColumnKey = "situacao";
    snapshot.sortAscending = true;
    snapshot.visibleColumns = {"numero_ssa", "situacao"};
    snapshot.columnWidths = {{"numero_ssa", 140}};
    snapshot.filters.searchText = "isolamento";
    snapshot.filters.quickSector = "IEE3";
    snapshot.filters.excludeScaSesSte = false;
    snapshot.filters.columnFilters = {{"situacao", "=ADM"}};
    snapshot.filters.advancedTextFilters = {{"setor_executor", "=MEG2"}, {"situacao", "=APV"}};
    snapshot.filters.advancedWeekColumnKey = "semana_executada";
    snapshot.filters.advancedYear = "2025";
    snapshot.filters.advancedWeek = "3";
    snapshot.filters.issueYear = "2026";
    snapshot.filters.executionYear = "2027";
    snapshot.filters.reprogrammingEquals = "2";
    snapshot.filters.reprogrammingMode = "lte";
    snapshot.filters.reprogrammingValues = "1,3,5";
    snapshot.filters.issueWeekStart = "202601";
    snapshot.filters.issueWeekEnd = "202620";
    snapshot.filters.executionWeekStart = "202701";
    snapshot.filters.executionWeekEnd = "202720";
    snapshot.filters.derivationMode = "derived";
    snapshot.filters.onlyReprogrammed = true;

    store.save(snapshot);
    const auto loaded = store.load();

    REQUIRE(loaded.pageSize == 50);
    REQUIRE(loaded.theme == "tokyo-night");
    REQUIRE(loaded.density == "compact");
    REQUIRE_FALSE(loaded.detailsVisible);
    REQUIRE(loaded.detailsPanelWidth == 520);
    REQUIRE(loaded.sortColumnKey == "situacao");
    REQUIRE(loaded.sortAscending);
    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
    REQUIRE(loaded.columnWidths.at("numero_ssa") == 140);
    REQUIRE(loaded.filters.searchText == "isolamento");
    REQUIRE(loaded.filters.quickSector == "IEE3");
    REQUIRE_FALSE(loaded.filters.excludeScaSesSte);
    REQUIRE(loaded.filters.columnFilters.at("situacao") == "=ADM");
    REQUIRE(loaded.filters.advancedTextFilters.at("setor_executor") == "=MEG2");
    REQUIRE(loaded.filters.advancedTextFilters.at("situacao") == "=APV");
    REQUIRE(loaded.filters.advancedWeekColumnKey == "semana_executada");
    REQUIRE(loaded.filters.advancedYear == "2025");
    REQUIRE(loaded.filters.advancedWeek == "3");
    REQUIRE(loaded.filters.issueYear == "2026");
    REQUIRE(loaded.filters.executionYear == "2027");
    REQUIRE(loaded.filters.reprogrammingEquals == "2");
    REQUIRE(loaded.filters.reprogrammingMode == "lte");
    REQUIRE(loaded.filters.reprogrammingValues == "1,3,5");
    REQUIRE(loaded.filters.issueWeekStart == "202601");
    REQUIRE(loaded.filters.issueWeekEnd == "202620");
    REQUIRE(loaded.filters.executionWeekStart == "202701");
    REQUIRE(loaded.filters.executionWeekEnd == "202720");
    REQUIRE(loaded.filters.derivationMode == "derived");
    REQUIRE(loaded.filters.onlyReprogrammed);
}

TEST_CASE("json preferences store keeps default columns when saved list is invalid") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"JSON({"visible_columns":[1,false,null]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.visibleColumns == ssa::domain::ColumnCatalog::defaultVisibleKeys());
}

TEST_CASE("json preferences store migrates legacy derived count visibility") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":1,"visible_columns":["numero_ssa","setor_executor","situacao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 2);
    REQUIRE(loaded.filters.quickSector == "IEE3");
    REQUIRE(loaded.visibleColumns ==
            std::vector<std::string>{"numero_ssa", "setor_executor", "qtd_derivadas", "situacao"});
}

TEST_CASE("json preferences store drops invalid column filters") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"JSON({"column_filters":{"missing":"x","situacao":"APV"}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE_FALSE(loaded.filters.columnFilters.contains("missing"));
    REQUIRE(loaded.filters.columnFilters.at("situacao") == "APV");
}

TEST_CASE("json preferences store drops invalid advanced text filters") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"JSON({"advanced_text_filters":{"missing":"x","setor_executor":"=MEG2"}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE_FALSE(loaded.filters.advancedTextFilters.contains("missing"));
    REQUIRE(loaded.filters.advancedTextFilters.at("setor_executor") == "=MEG2");
}

TEST_CASE("json filter preset store saves only filter state") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "preset.json";
    const ssa::infra::preferences::JsonFilterPresetStore store;

    ssa::ports::FilterPresetSnapshot snapshot;
    snapshot.filters.quickSector = "MEG2";
    snapshot.filters.excludeScaSesSte = false;
    snapshot.filters.columnFilters = {{"situacao", "=APV"}};
    snapshot.filters.advancedTextFilters = {{"setor_executor", "=MMU3"}};
    snapshot.filters.issueYear = "2026";
    snapshot.filters.onlyReprogrammed = true;

    store.save(path, snapshot);
    const auto loaded = store.load(path);

    REQUIRE(loaded.filters.quickSector == "MEG2");
    REQUIRE_FALSE(loaded.filters.excludeScaSesSte);
    REQUIRE(loaded.filters.columnFilters.at("situacao") == "=APV");
    REQUIRE(loaded.filters.advancedTextFilters.at("setor_executor") == "=MMU3");
    REQUIRE(loaded.filters.issueYear == "2026");
    REQUIRE(loaded.filters.onlyReprogrammed);
}
