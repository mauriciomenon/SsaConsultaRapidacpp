#include "domain/ColumnCatalog.h"
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
    snapshot.theme = "dark";
    snapshot.density = "compact";
    snapshot.detailsVisible = false;
    snapshot.detailsPanelWidth = 520;
    snapshot.sortColumnKey = "situacao";
    snapshot.sortAscending = true;
    snapshot.visibleColumns = {"numero_ssa", "situacao"};
    snapshot.columnWidths = {{"numero_ssa", 140}};
    snapshot.quickSector = "IEE3";
    snapshot.excludeScaSesSte = false;
    snapshot.columnFilters = {{"situacao", "=ADM"}};
    snapshot.advancedWeekColumnKey = "semana_executada";
    snapshot.advancedYear = "2025";
    snapshot.advancedWeek = "3";
    snapshot.derivationMode = "derived";
    snapshot.onlyReprogrammed = true;

    store.save(snapshot);
    const auto loaded = store.load();

    REQUIRE(loaded.pageSize == 50);
    REQUIRE(loaded.theme == "dark");
    REQUIRE(loaded.density == "compact");
    REQUIRE_FALSE(loaded.detailsVisible);
    REQUIRE(loaded.detailsPanelWidth == 520);
    REQUIRE(loaded.sortColumnKey == "situacao");
    REQUIRE(loaded.sortAscending);
    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
    REQUIRE(loaded.columnWidths.at("numero_ssa") == 140);
    REQUIRE(loaded.quickSector == "IEE3");
    REQUIRE_FALSE(loaded.excludeScaSesSte);
    REQUIRE(loaded.columnFilters.at("situacao") == "=ADM");
    REQUIRE(loaded.advancedWeekColumnKey == "semana_executada");
    REQUIRE(loaded.advancedYear == "2025");
    REQUIRE(loaded.advancedWeek == "3");
    REQUIRE(loaded.derivationMode == "derived");
    REQUIRE(loaded.onlyReprogrammed);
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

    REQUIRE_FALSE(loaded.columnFilters.contains("missing"));
    REQUIRE(loaded.columnFilters.at("situacao") == "APV");
}
