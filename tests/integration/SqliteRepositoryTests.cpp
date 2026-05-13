#include "domain/ColumnCatalog.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteMaintenancePort.h"
#include "infra/sqlite/SqliteSsaRepository.h"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QTemporaryFile>

#include <filesystem>
#include <sqlite3.h>
#include <string>

namespace {

    std::filesystem::path createFixture() {
        QTemporaryFile dbFile(QDir::tempPath() + QStringLiteral("/ssa_cpp_fixture_XXXXXX.sqlite"));
        dbFile.setAutoRemove(true);
        REQUIRE(dbFile.open());
        const auto path = std::filesystem::path(dbFile.fileName().toStdString());
        dbFile.close();

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
            semana_executada INTEGER,
            num_reprogramacoes INTEGER,
            total_de_reprogramacoes INTEGER
        );
        INSERT INTO ssa_table VALUES
            ('202500003','APV','202400001','LOC-1','Casa de forca','EQ-A',202501,'2025-01-01','Trocar filtro','Executar troca','SEM','SMM','Ana','Bruno','Caio','SAM','SYS','a.xlsx','2025-01-01','A','B',202502,202503,1,1),
            ('202500002','STE','','LOC-2','Vertedouro','EQ-B',202501,'2025-01-02','Inspecionar bomba','Inspecao','SEM','STE','Bia','Davi','Eva','SAM','SYS','b.xlsx','2025-01-02','C','D',202502,202503,0,0),
            ('202500001','SES','','LOC-3','Patio','EQ-C',202501,'2025-01-03','Limpar painel','Limpeza','OPR','SMM','Ivo','Leo','Mia','SAM','SYS','c.xlsx','2025-01-03','E','F',202502,202503,0,0),
            ('202500000','SCA','','LOC-4','Galeria','EQ-D',202501,'2025-01-04','Cancelar atividade','Cancelada','OPR','SMM','Noa','Lia','Rui','SAM','SYS','d.xlsx','2025-01-04','G','H',202502,202503,0,0);
    )SQL";
        char* error = nullptr;
        const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
        if (error != nullptr) {
            sqlite3_free(error);
        }
        REQUIRE(rc == SQLITE_OK);
        sqlite3_close(db);
        dbFile.setAutoRemove(false);
        return path;
    }

    struct SqliteFixture {
        std::filesystem::path path{createFixture()};

        SqliteFixture() = default;

        ~SqliteFixture() {
            std::filesystem::remove(path);
        }

        SqliteFixture(const SqliteFixture&) = delete;
        SqliteFixture& operator=(const SqliteFixture&) = delete;
        SqliteFixture(SqliteFixture&&) = delete;
        SqliteFixture& operator=(SqliteFixture&&) = delete;
    };

    struct SqliteRepositoryFixture : SqliteFixture {
        ssa::infra::sqlite::SqliteSsaRepository repository{path};
    };

} // namespace

TEST_CASE_METHOD(SqliteRepositoryFixture, "sqlite repository pages and filters rows") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.searchText = "filtro";

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows.size() == 1);
    REQUIRE(page.rows[0].valueOf("numero_ssa") == "202500003");
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository applies safe pattern through escaped LIKE") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.searchText = "~trocar filtro";

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows[0].valueOf("numero_ssa") == "202500003");
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository negates general search across all searched columns") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.searchText = "!filtro";
    request.excludeScaSesSte = false;

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 3);
    REQUIRE(page.rows.size() == 3);
    for (const auto& row : page.rows) {
        REQUIRE(row.valueOf("numero_ssa") != "202500003");
    }
}

TEST_CASE_METHOD(SqliteRepositoryFixture, "sqlite repository excludes SCA SES STE by default") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows.size() == 1);
    REQUIRE(page.rows[0].valueOf("numero_ssa") == "202500003");
    REQUIRE(page.rows[0].valueOf("situacao") == "APV");
}

TEST_CASE_METHOD(SqliteRepositoryFixture, "sqlite repository returns details and distinct values") {
    const auto record = repository.recordBySsaNumber(ssa::domain::SsaNumber{"202500003"});
    REQUIRE(record.has_value());
    REQUIRE(record->valueOf("setor_executor") == "SMM");

    ssa::domain::DistinctValuesRequest request;
    request.columnKey = "setor_executor";
    const auto values = repository.distinctValues(request);
    REQUIRE_FALSE(values.empty());
}

TEST_CASE_METHOD(SqliteRepositoryFixture, "sqlite repository applies advanced filters") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.advancedFilters.weekColumnKey = "semana_programada";
    request.advancedFilters.year = 2025;
    request.advancedFilters.week = 2;
    request.advancedFilters.derivationMode = ssa::domain::DerivationFilterMode::DerivedOnly;
    request.advancedFilters.onlyReprogrammed = true;

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows[0].valueOf("numero_ssa") == "202500003");
}

TEST_CASE("sqlite maintenance port runs vacuum analyze") {
    const SqliteFixture fixture;
    ssa::infra::sqlite::SqliteMaintenancePort maintenance(fixture.path);

    const auto result = maintenance.vacuumAnalyze();

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
}

TEST_CASE("sqlite maintenance port resets table data") {
    const SqliteFixture fixture;
    ssa::infra::sqlite::SqliteMaintenancePort maintenance(fixture.path);

    const auto result = maintenance.resetDatabase();
    const ssa::infra::sqlite::SqliteSsaRepository repository(fixture.path);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(repository.count({}) == 0);
}
