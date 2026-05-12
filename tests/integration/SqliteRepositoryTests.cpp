#include "infra/preferences/JsonUserPreferencesStore.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteSsaRepository.h"

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>

#include <filesystem>
#include <sqlite3.h>

namespace {

    std::filesystem::path createFixture() {
        const auto path = std::filesystem::temp_directory_path() / "ssa_cpp_fixture.sqlite";
        std::filesystem::remove(path);

        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
        const char* sql = R"SQL(
        CREATE TABLE ssa_table (
            numero_ssa TEXT,
            situacao TEXT,
            derivada_de TEXT,
            localizacao_codigo TEXT,
            descricao_localizacao TEXT,
            equipamento TEXT,
            semana_cadastro INTEGER,
            data_cadastro TEXT,
            descricao_ssa TEXT,
            descricao_execucao TEXT,
            setor_emissor TEXT,
            setor_executor TEXT,
            solicitante TEXT,
            responsavel_programacao TEXT,
            responsavel_execucao TEXT,
            servico_origem TEXT,
            sistema_origem TEXT,
            arquivo_origem TEXT,
            data_planilha TEXT,
            grau_prioridade_emissao TEXT,
            grau_prioridade_planejamento TEXT,
            semana_programada INTEGER,
            semana_executada INTEGER
        );
        INSERT INTO ssa_table VALUES
            ('202500003','APV','','LOC-1','Casa de forca','EQ-A',202501,'2025-01-01','Trocar filtro','Executar troca','SEM','SMM','Ana','Bruno','Caio','SAM','SYS','a.xlsx','2025-01-01','A','B',202502,202503),
            ('202500002','STE','','LOC-2','Vertedouro','EQ-B',202501,'2025-01-02','Inspecionar bomba','Inspecao','SEM','STE','Bia','Davi','Eva','SAM','SYS','b.xlsx','2025-01-02','C','D',202502,202503),
            ('202500001','SES','','LOC-3','Patio','EQ-C',202501,'2025-01-03','Limpar painel','Limpeza','OPR','SMM','Ivo','Leo','Mia','SAM','SYS','c.xlsx','2025-01-03','E','F',202502,202503);
    )SQL";
        char* error = nullptr;
        REQUIRE(sqlite3_exec(db, sql, nullptr, nullptr, &error) == SQLITE_OK);
        sqlite3_close(db);
        return path;
    }

} // namespace

TEST_CASE("sqlite repository pages and filters rows") {
    const auto path = createFixture();
    const ssa::infra::sqlite::SqliteSsaRepository repository(path);

    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.searchText = "filtro";

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows.size() == 1);
    REQUIRE(page.rows[0].valueOf("numero_ssa") == "202500003");
}

TEST_CASE("sqlite repository excludes closed statuses by default") {
    const auto path = createFixture();
    const ssa::infra::sqlite::SqliteSsaRepository repository(path);

    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows[0].valueOf("situacao") == "APV");
}

TEST_CASE("sqlite repository returns details and distinct values") {
    const auto path = createFixture();
    const ssa::infra::sqlite::SqliteSsaRepository repository(path);

    const auto record = repository.recordById(ssa::domain::SsaId{"202500003"});
    REQUIRE(record.has_value());
    REQUIRE(record->valueOf("setor_executor") == "SMM");

    ssa::domain::DistinctValuesRequest request;
    request.columnKey = "setor_executor";
    const auto values = repository.distinctValues(request);
    REQUIRE_FALSE(values.empty());
}

TEST_CASE("json preferences store saves versioned gui state") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto path = std::filesystem::path{directory.path().toStdString()} / "prefs.json";
    const ssa::infra::preferences::JsonUserPreferencesStore store(path);

    ssa::ports::UserPreferencesSnapshot snapshot;
    snapshot.pageSize = 50;
    snapshot.theme = "dark";
    snapshot.detailsVisible = false;
    snapshot.visibleColumns = {"numero_ssa", "situacao"};
    snapshot.columnWidths = {{"numero_ssa", 140}};
    snapshot.quickSector = "IEE3";
    snapshot.excludeClosedStatuses = false;
    snapshot.columnFilters = {{"situacao", "=ADM"}};

    store.save(snapshot);
    const auto loaded = store.load();

    REQUIRE(loaded.pageSize == 50);
    REQUIRE(loaded.theme == "dark");
    REQUIRE_FALSE(loaded.detailsVisible);
    REQUIRE(loaded.visibleColumns == std::vector<std::string>{"numero_ssa", "situacao"});
    REQUIRE(loaded.columnWidths.at("numero_ssa") == 140);
    REQUIRE(loaded.quickSector == "IEE3");
    REQUIRE_FALSE(loaded.excludeClosedStatuses);
    REQUIRE(loaded.columnFilters.at("situacao") == "=ADM");
}
