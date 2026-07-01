#include "domain/ColumnCatalog.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDerivadasPort.h"
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

    void executeSql(const std::filesystem::path& path, const char* sql) {
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
        char* error = nullptr;
        const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
        if (error != nullptr) {
            sqlite3_free(error);
        }
        sqlite3_close(db);
        REQUIRE(rc == SQLITE_OK);
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

TEST_CASE_METHOD(SqliteRepositoryFixture, "sqlite repository excludes SCA SES STE when requested") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.excludeScaSesSte = true;

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

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository projects derived count virtual column") {
    executeSql(path, R"SQL(
        INSERT INTO ssa_table VALUES
            ('202500004','APV','202500002','LOC-5','Tomada dagua','EQ-E',202501,'2025-01-05','Ajustar valvula','Ajuste','SEM','SMM','Ari','Beto','Clio','SAM','SYS','e.xlsx','2025-01-05','I','J',202502,202503,0,0);
    )SQL");

    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.excludeScaSesSte = false;
    request.visibleColumns = {"numero_ssa",
                              std::string{ssa::domain::ColumnCatalog::derivedCountColumnKey()}};

    const auto page = repository.page(request);

    const auto parent = std::ranges::find_if(page.rows, [](const ssa::domain::SsaRecord& row) {
        return row.valueOf("numero_ssa") == "202500002";
    });
    REQUIRE(parent != page.rows.end());
    REQUIRE(parent->valueOf("qtd_derivadas") == "1");

    const auto child = std::ranges::find_if(page.rows, [](const ssa::domain::SsaRecord& row) {
        return row.valueOf("numero_ssa") == "202500004";
    });
    REQUIRE(child != page.rows.end());
    REQUIRE(child->valueOf("qtd_derivadas") == "0");
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository readAll streams pages when page sized") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 2;
    request.excludeScaSesSte = false;

    std::vector<std::string> seen;
    const auto result = repository.readAll(request, [&seen](const ssa::domain::SsaRecord& row) {
        seen.emplace_back(row.valueOf("numero_ssa"));
        return std::nullopt;
    });

    REQUIRE(result.ok());
    REQUIRE(result.rowCount == 4);
    REQUIRE(seen.size() == 4);
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository readAll unbounded when page size zero") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 0;
    request.excludeScaSesSte = false;

    std::size_t count = 0;
    const auto result = repository.readAll(request, [&count](const ssa::domain::SsaRecord&) {
        ++count;
        return std::nullopt;
    });

    REQUIRE(result.ok());
    REQUIRE(count == 4);
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

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository applies advanced text and range filters") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.advancedFilters.textFilters = {{"situacao", "=APV"}, {"setor_executor", "=SMM"}};
    request.advancedFilters.issueYear = 2025;
    request.advancedFilters.executionYear = 2025;
    request.advancedFilters.reprogrammingEquals = 1;
    request.advancedFilters.issueWeekStart = 202501;
    request.advancedFilters.issueWeekEnd = 202501;
    request.advancedFilters.executionWeekStart = 202503;
    request.advancedFilters.executionWeekEnd = 202503;

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows.size() == 1);
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

TEST_CASE("sqlite derivadas port clears orphan derivadas") {
    const SqliteFixture fixture;
    ssa::infra::sqlite::SqliteDerivadasPort derivadasPort(fixture.path);
    ssa::infra::sqlite::SqliteSsaRepository repository(fixture.path);

    REQUIRE(repository.recordBySsaNumber(ssa::domain::SsaNumber{"202500003"}).has_value());
    REQUIRE(
        repository.recordBySsaNumber(ssa::domain::SsaNumber{"202500003"})->valueOf("derivada_de") ==
        "202400001");

    const auto result = derivadasPort.syncDerivadas();

    const auto after = repository.recordBySsaNumber(ssa::domain::SsaNumber{"202500003"});
    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(after.has_value());
    REQUIRE(after->valueOf("derivada_de").empty());
}
