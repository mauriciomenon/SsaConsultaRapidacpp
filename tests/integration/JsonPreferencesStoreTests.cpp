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
    snapshot.savedFilters = {ssa::ports::SavedFilterSnapshot{
        .name = "braba",
        .filters =
            ssa::ports::FilterPreferencesSnapshot{.searchText = "svp", .quickSector = "MEG2"}}};
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
    REQUIRE(loaded.savedFilters.size() == 1);
    REQUIRE(loaded.savedFilters.front().name == "braba");
    REQUIRE(loaded.savedFilters.front().filters.searchText == "svp");
    REQUIRE(loaded.savedFilters.front().filters.quickSector == "MEG2");
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

TEST_CASE("json preferences store prunes legacy hidden visible columns on load") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"visible_columns":["numero_ssa","equipamento","grau_prioridade_emissao","grau_prioridade_planejamento","situacao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
}

TEST_CASE("json preferences store prunes legacy description location visibility on load") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"JSON({"visible_columns":["numero_ssa","descricao_localizacao","situacao"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
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

    REQUIRE(loaded.schemaVersion == 8);
    REQUIRE(loaded.filters.quickSector == "IEE3");
    REQUIRE(loaded.visibleColumns ==
            std::vector<std::string>{"numero_ssa", "setor_executor", "qtd_derivadas", "situacao"});
}

TEST_CASE("json preferences store restores default quick sector when it is alone") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":3,"quick_sector":"","visible_columns":["numero_ssa","setor_executor","qtd_derivadas"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 8);
    REQUIRE(loaded.filters.quickSector == "IEE3");
}

TEST_CASE("json preferences store keeps empty quick sector when other filters exist") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":3,"quick_sector":"","search_text":"manual","visible_columns":["numero_ssa","setor_executor","qtd_derivadas"]})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 8);
    REQUIRE(loaded.filters.quickSector.empty());
    REQUIRE(loaded.filters.searchText == "manual");
}

TEST_CASE("json preferences store migrates only legacy default table widths") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":4,"column_widths":{"numero_ssa":110,"situacao":80,"localizacao_codigo":120,"setor_emissor":90,"setor_executor":91,"qtd_derivadas":72,"derivada_de":82,"data_cadastro":120,"semana_cadastro":95,"descricao_ssa":360,"solicitante":180,"responsavel_programacao":180,"responsavel_execucao":180,"semana_programada":95,"semana_executada":95}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 8);
    REQUIRE(loaded.columnWidths.at("numero_ssa") == 98);
    REQUIRE(loaded.columnWidths.at("situacao") == 60);
    REQUIRE(loaded.columnWidths.at("localizacao_codigo") == 84);
    REQUIRE(loaded.columnWidths.at("setor_emissor") == 72);
    REQUIRE(loaded.columnWidths.at("setor_executor") == 91);
    REQUIRE(loaded.columnWidths.at("qtd_derivadas") == 70);
    REQUIRE(loaded.columnWidths.at("derivada_de") == 90);
    REQUIRE(loaded.columnWidths.at("data_cadastro") == 100);
    REQUIRE(loaded.columnWidths.at("semana_cadastro") == 84);
    REQUIRE(loaded.columnWidths.at("descricao_ssa") == 640);
    REQUIRE(loaded.columnWidths.at("solicitante") == 240);
    REQUIRE(loaded.columnWidths.at("responsavel_programacao") == 250);
    REQUIRE(loaded.columnWidths.at("responsavel_execucao") == 250);
    REQUIRE(loaded.columnWidths.at("semana_programada") == 86);
    REQUIRE(loaded.columnWidths.at("semana_executada") == 86);
}

TEST_CASE("json preferences store compacts oversized derivation column width") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"JSON({"schema_version":7,"column_widths":{"derivada_de":220,"descricao_ssa":640}})JSON");
    file.close();

    const ssa::infra::preferences::JsonUserPreferencesStore store(path);
    const auto loaded = store.load();

    REQUIRE(loaded.schemaVersion == 8);
    REQUIRE(loaded.columnWidths.at("derivada_de") == 90);
    REQUIRE(loaded.columnWidths.at("descricao_ssa") == 640);
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
