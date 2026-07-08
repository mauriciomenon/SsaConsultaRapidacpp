#include "domain/ColumnCatalog.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDerivadasPort.h"
#include "infra/sqlite/SqliteMaintenancePort.h"
#include "infra/sqlite/SqliteSsaRepository.h"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QTemporaryFile>

#include <algorithm>
#include <filesystem>
#include <sqlite3.h>
#include <string>
#include <vector>

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

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository applies positive and negative filters on same column") {
    ssa::domain::SsaPageRequest request;
    request.pageSize = 10;
    request.columnFilters = {{"situacao", "=APV"}};
    request.advancedFilters.textFilters = {{"situacao", "!STE"}};

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows.size() == 1);
    REQUIRE(page.rows[0].valueOf("situacao") == "APV");

    request.advancedFilters.textFilters = {{"situacao", "!APV"}};
    const auto contradictoryPage = repository.page(request);

    REQUIRE(contradictoryPage.totalRows == 0);
    REQUIRE(contradictoryPage.rows.empty());
}

TEST_CASE_METHOD(SqliteRepositoryFixture, "sqlite repository returns details and distinct values") {
    const auto record = repository.recordBySsaNumber(ssa::domain::SsaNumber{"202500003"});
    REQUIRE(record.has_value());
    REQUIRE(record->valueOf("setor_executor") == "SMM");

    executeSql(path, R"SQL(
        INSERT INTO ssa_table VALUES
            ('202500010','APV','','LOC-10','Area','EQ-X',202501,'2025-01-10','A','A','SEM','MEG2','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-10','A','B',202502,202503,0,0),
            ('202500011','APV','','LOC-11','Area','EQ-X',202501,'2025-01-11','A','A','SEM','IEE4','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-11','A','B',202502,202503,0,0),
            ('202500012','APV','','LOC-12','Area','EQ-X',202501,'2025-01-12','A','A','SEM','MEL2','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-12','A','B',202502,202503,0,0),
            ('202500013','APV','','LOC-13','Area','EQ-X',202501,'2025-01-13','A','A','SEM','IEE3','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-13','A','B',202502,202503,0,0),
            ('202500017','APV','','LOC-17','Area','EQ-X',202501,'2025-01-17','A','A','SEM','IEE2','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-17','A','B',202502,202503,0,0),
            ('202500018','APV','','LOC-18','Area','EQ-X',202501,'2025-01-18','A','A','SEM','MEL3','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-18','A','B',202502,202503,0,0),
            ('202500019','APV','','LOC-19','Area','EQ-X',202501,'2025-01-19','A','A','SEM','MEL4','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-19','A','B',202502,202503,0,0),
            ('202500014','APV','','LOC-14','Area','EQ-X',202501,'2025-01-14','A','A','SEM','AAA','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-14','A','B',202502,202503,0,0),
            ('202500015','APV','','LOC-15','Area','EQ-X',202501,'2025-01-15','A','A','SEM','MEL1','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-15','A','B',202502,202503,0,0),
            ('202500016','APV','','LOC-16','Area','EQ-X',202501,'2025-01-16','A','A','SEM','IEE1','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-16','A','B',202502,202503,0,0);
    )SQL");

    ssa::domain::DistinctValuesRequest request;
    request.columnKey = "setor_executor";
    const auto values = repository.distinctValues(request);
    REQUIRE(values == std::vector<std::string>{"IEE3", "IEE1", "IEE2", "IEE4", "MEL1", "MEL2",
                                               "MEL3", "MEL4", "AAA", "MEG2", "SMM", "STE"});
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository orders responsible and numeric distinct values for display") {
    executeSql(path, R"SQL(
        INSERT INTO ssa_table VALUES
            ('202500020','APV','','LOC-20','Area','EQ-X',202501,'2025-01-20','A','A','SEM','IEE2','Ana','Bruno','IEE2 BRUNO','SAM','SYS','x.xlsx','2025-01-20','A','B',202502,202503,12,12),
            ('202500021','APV','','LOC-21','Area','EQ-X',202501,'2025-01-21','A','A','SEM','MEG2','Ana','Bruno','MARIA','SAM','SYS','x.xlsx','2025-01-21','A','B',202502,202503,2,2),
            ('202500022','APV','','LOC-22','Area','EQ-X',202501,'2025-01-22','A','A','SEM','MEG2','Ana','Bruno','IEE3 ANA','SAM','SYS','x.xlsx','2025-01-22','A','B',202502,202503,7,7),
            ('202500023','APV','','LOC-23','Area','EQ-X',202501,'2025-01-23','A','A','SEM','MEG2','Ana','Bruno','MEL1 CAIO','SAM','SYS','x.xlsx','2025-01-23','A','B',202502,202503,3,3),
            ('202500024','APV','','LOC-24','Area','EQ-X',202501,'2025-01-24','A','A','SEM','MEG2','Ana','Bruno','IEE1 DORA','SAM','SYS','x.xlsx','2025-01-24','A','B',202502,202503,9,9),
            ('202500025','APV','','LOC-25','Area','EQ-X',202501,'2025-01-25','A','A','SEM','IEE3','Ana','Bruno','ZE IEE3','SAM','SYS','x.xlsx','2025-01-25','A','B',202502,202503,1,1),
            ('202500026','APV','','LOC-26','Area','EQ-X',202501,'2025-01-26','A','A','SEM','IEE1','Ana','Bruno','ZE IEE1','SAM','SYS','x.xlsx','2025-01-26','A','B',202502,202503,0,0),
            ('202500027','APV','','LOC-27','Area','EQ-X',202501,'2025-01-27','A','A','SEM','IEE3','Ana','Bruno','CARLOS SETOR','SAM','SYS','x.xlsx','2025-01-27','A','B',202502,202503,4,4),
            ('202500028','APV','','LOC-28','Area','EQ-X',202501,'2025-01-28','A','A','SEM','IEE1','Ana','Bruno','ALAN SETOR','SAM','SYS','x.xlsx','2025-01-28','A','B',202502,202503,5,5),
            ('202500029','APV','','LOC-29','Area','EQ-X',202501,'2025-01-29','A','A','SEM','MEL1','Ana','Bruno','BRUNO SETOR','SAM','SYS','x.xlsx','2025-01-29','A','B',202502,202503,6,6),
            ('202500030','APV','','LOC-30','Area','EQ-X',202501,'2025-01-30','A','A','SEM','AAA','Ana','Bruno','AARON SETOR','SAM','SYS','x.xlsx','2025-01-30','A','B',202502,202503,8,8);
    )SQL");

    ssa::domain::DistinctValuesRequest peopleRequest;
    peopleRequest.columnKey = "responsavel_execucao";
    const auto people = repository.distinctValues(peopleRequest);
    REQUIRE(people == std::vector<std::string>{"IEE3 ANA", "IEE1 DORA", "IEE2 BRUNO", "MEL1 CAIO",
                                               "CARLOS SETOR", "ZE IEE3", "ALAN SETOR", "ZE IEE1",
                                               "BRUNO SETOR", "AARON SETOR", "Caio", "Eva", "MARIA",
                                               "Mia", "Rui"});
    const auto zeIee3 = std::ranges::find(people, "ZE IEE3");
    const auto zeIee1 = std::ranges::find(people, "ZE IEE1");
    REQUIRE(zeIee3 != people.end());
    REQUIRE(zeIee1 != people.end());
    REQUIRE(zeIee3 < zeIee1);
    const auto carlos = std::ranges::find(people, "CARLOS SETOR");
    const auto alan = std::ranges::find(people, "ALAN SETOR");
    const auto bruno = std::ranges::find(people, "BRUNO SETOR");
    const auto aaron = std::ranges::find(people, "AARON SETOR");
    REQUIRE(carlos != people.end());
    REQUIRE(alan != people.end());
    REQUIRE(bruno != people.end());
    REQUIRE(aaron != people.end());
    REQUIRE(carlos < alan);
    REQUIRE(alan < bruno);
    REQUIRE(bruno < aaron);

    ssa::domain::DistinctValuesRequest numericRequest;
    numericRequest.columnKey = "num_reprogramacoes";
    const auto numbers = repository.distinctValues(numericRequest);
    REQUIRE(numbers ==
            std::vector<std::string>{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "12"});
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
