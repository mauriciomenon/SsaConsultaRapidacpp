#include "domain/ColumnCatalog.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDerivadasPort.h"
#include "infra/sqlite/SqliteMaintenancePort.h"
#include "infra/sqlite/SqliteProgressHandler.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "qt/FilesystemPath.h"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <mutex>
#include <semaphore>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <system_error>
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

    bool hasIndex(const std::filesystem::path& path, const char* indexName) {
        ssa::infra::sqlite::SqliteConnection connection(path);
        ssa::infra::sqlite::SqliteStatement statement(
            connection.handle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND name = ?");
        statement.bindTextOneBased(1, indexName);
        REQUIRE(statement.step());
        return statement.columnInt64(0) == 1;
    }

    struct SqliteFixture {
        std::filesystem::path path = createFixture();

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
        SqliteRepositoryFixture() : repository(path) {}

        ssa::infra::sqlite::SqliteSsaRepository repository;
    };

} // namespace

TEST_CASE("sqlite progress handler interrupts after entering sqlite and remains reusable") {
    const SqliteFixture fixture;
    ssa::infra::sqlite::SqliteConnection connection(fixture.path,
                                                    ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
    std::stop_source stopSource;
    std::binary_semaphore progressEntered(0);

    auto query = std::async(std::launch::async, [&] {
        ssa::infra::sqlite::SqliteProgressHandler progress(
            connection.handle(), stopSource.get_token(), &progressEntered);
        char* error = nullptr;
        const int result = sqlite3_exec(
            connection.handle(),
            "WITH RECURSIVE numbers(value) AS (VALUES(0) UNION ALL SELECT value + 1 FROM "
            "numbers WHERE value < 100000000) SELECT sum(value) FROM numbers",
            nullptr, nullptr, &error);
        sqlite3_free(error);
        return result;
    });

    progressEntered.acquire();
    stopSource.request_stop();

    REQUIRE(query.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    REQUIRE(query.get() == SQLITE_INTERRUPT);
    ssa::infra::sqlite::SqliteStatement reuse(connection.handle(), "SELECT 1");
    REQUIRE(reuse.step());
    REQUIRE(reuse.columnInt64(0) == 1);
}

TEST_CASE("sqlite connection preserves unicode database paths") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto databasePath = ssa::qt::toFileSystemPath(
        directory.filePath(QString::fromUtf8("dados-unicode-\xE6\xBC\xA2/ssas.db")));
    std::filesystem::create_directories(databasePath.parent_path());
    {
        ssa::infra::sqlite::SqliteConnection connection(
            databasePath, ssa::infra::sqlite::SqliteOpenMode::ReadWriteCreate);
        REQUIRE(sqlite3_exec(connection.handle(),
                             "CREATE TABLE unicode_probe(value TEXT);"
                             "INSERT INTO unicode_probe VALUES('ok');",
                             nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    ssa::infra::sqlite::SqliteConnection reopened(databasePath);
    ssa::infra::sqlite::SqliteStatement statement(reopened.handle(),
                                                  "SELECT value FROM unicode_probe");
    REQUIRE(statement.step());
    REQUIRE(statement.columnText(0) == "ok");
}

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
                 "sqlite repository measures trimmed maximum value length") {
    executeSql(path, R"SQL(
        UPDATE ssa_table
        SET responsavel_execucao = '  Longest Name  '
        WHERE numero_ssa = '202500003';
    )SQL");

    REQUIRE(repository.maxValueLength("responsavel_execucao") == 12);
    REQUIRE(repository.maxValueLength("situacao") == 3);
    REQUIRE_THROWS_AS(repository.maxValueLength("unknown_column"), std::invalid_argument);
    REQUIRE_THROWS_AS(repository.maxValueLength("qtd_derivadas"), std::invalid_argument);
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository rejects maximum length query with stopped token") {
    std::stop_source stopSource;
    stopSource.request_stop();

    try {
        static_cast<void>(repository.maxValueLength("situacao", stopSource.get_token()));
        FAIL("stopped maximum length query did not report cancellation");
    } catch (const std::system_error& error) {
        REQUIRE(error.code() == std::make_error_code(std::errc::operation_canceled));
    }
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
                 "sqlite repository orders status distinct values alphabetically") {
    executeSql(path, R"SQL(
        INSERT INTO ssa_table VALUES
            ('202500031','STE','','LOC-31','Area','EQ-X',202501,'2025-01-31','A','A','SEM','IEE3','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-01-31','A','B',202502,202503,0,0),
            ('202500032','AAD','','LOC-32','Area','EQ-X',202501,'2025-02-01','A','A','SEM','IEE3','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-02-01','A','B',202502,202503,0,0),
            ('202500033','APV','','LOC-33','Area','EQ-X',202501,'2025-02-02','A','A','SEM','IEE3','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-02-02','A','B',202502,202503,0,0),
            ('202500034','ADM','','LOC-34','Area','EQ-X',202501,'2025-02-03','A','A','SEM','IEE3','Ana','Bruno','Caio','SAM','SYS','x.xlsx','2025-02-03','A','B',202502,202503,0,0);
    )SQL");

    ssa::domain::DistinctValuesRequest request;
    request.columnKey = "situacao";
    request.filter.excludeScaSesSte = false;

    const auto values = repository.distinctValues(request);

    REQUIRE(values == std::vector<std::string>{"AAD", "ADM", "APV", "SCA", "SES", "STE"});
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

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository keeps page reads available while streaming export") {
    ssa::domain::SsaPageRequest request;
    request.excludeScaSesSte = false;
    request.pageSize = 0;
    std::mutex mutex;
    std::condition_variable enteredCondition;
    std::condition_variable releaseCondition;
    bool entered = false;
    bool release = false;

    auto exportRead = std::async(std::launch::async, [&] {
        return repository.readAll(request, [&](const ssa::domain::SsaRecord&) {
            std::unique_lock lock(mutex);
            entered = true;
            enteredCondition.notify_one();
            releaseCondition.wait(lock, [&] { return release; });
            return std::optional<std::string>{};
        });
    });
    struct ReleaseGuard final {
        std::mutex& mutex;
        std::condition_variable& condition;
        bool& release;

        ~ReleaseGuard() {
            {
                const std::scoped_lock lock(mutex);
                release = true;
            }
            condition.notify_one();
        }
    } releaseGuard{mutex, releaseCondition, release};
    {
        std::unique_lock lock(mutex);
        REQUIRE(enteredCondition.wait_for(lock, std::chrono::seconds{1}, [&] { return entered; }));
    }

    auto count = std::async(std::launch::async, [&] { return repository.count(request); });
    const bool countReady =
        count.wait_for(std::chrono::milliseconds{100}) == std::future_status::ready;
    {
        const std::scoped_lock lock(mutex);
        release = true;
    }
    releaseCondition.notify_one();
    REQUIRE(exportRead.get().ok());
    REQUIRE(countReady);
    REQUIRE(count.get() > 0);
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
    request.advancedFilters.reprogrammingValues = {1};
    request.advancedFilters.issueWeekStart = 202501;
    request.advancedFilters.issueWeekEnd = 202501;
    request.advancedFilters.executionWeekStart = 202503;
    request.advancedFilters.executionWeekEnd = 202503;

    const auto page = repository.page(request);

    REQUIRE(page.totalRows == 1);
    REQUIRE(page.rows.size() == 1);
    REQUIRE(page.rows[0].valueOf("numero_ssa") == "202500003");
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository rejects a query with a stopped token") {
    std::stop_source stopSource;
    stopSource.request_stop();

    ssa::domain::SsaPageRequest request;
    request.excludeScaSesSte = false;

    try {
        static_cast<void>(repository.count(request, stopSource.get_token()));
        FAIL("stopped query did not report cancellation");
    } catch (const std::system_error& error) {
        REQUIRE(error.code() == std::make_error_code(std::errc::operation_canceled));
    }
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository rejects every remaining read with a stopped token") {
    std::stop_source stopSource;
    stopSource.request_stop();
    const auto token = stopSource.get_token();
    ssa::domain::SsaPageRequest request;

    const auto requireCanceled = [](auto&& operation) {
        try {
            operation();
            FAIL("stopped read did not report cancellation");
        } catch (const std::system_error& error) {
            REQUIRE(error.code() == std::make_error_code(std::errc::operation_canceled));
        }
    };

    requireCanceled([&] {
        static_cast<void>(repository.recordBySsaNumber(ssa::domain::SsaNumber{"202500003"}, token));
    });
    requireCanceled([&] {
        static_cast<void>(repository.derivadasDiretas(ssa::domain::SsaNumber{"202500003"}, token));
    });
    requireCanceled([&] {
        static_cast<void>(repository.readAll(
            request, [](const ssa::domain::SsaRecord&) { return std::nullopt; }, token));
    });
}

TEST_CASE_METHOD(SqliteRepositoryFixture,
                 "sqlite repository interrupts a running query after stop") {
    executeSql(path, R"SQL(
        CREATE TABLE slow_query_control (max_value INTEGER NOT NULL);
        INSERT INTO slow_query_control VALUES (100000000);
        CREATE VIEW slow_ssa_table AS
        WITH RECURSIVE counter(value) AS (
            VALUES(1)
            UNION ALL
            SELECT value + 1 FROM counter
            WHERE value < (SELECT max_value FROM slow_query_control)
        )
        SELECT value AS numero_ssa FROM counter;
    )SQL");
    ssa::infra::sqlite::SqliteSsaRepository slowRepository(
        path, ssa::query::SqlQueryBuilder{"slow_ssa_table"});
    ssa::domain::SsaPageRequest request;
    request.excludeScaSesSte = false;
    std::stop_source stopSource;

    auto query = std::async(std::launch::async, [&] {
        try {
            static_cast<void>(slowRepository.page(request, stopSource.get_token()));
            return std::error_code{};
        } catch (const std::system_error& error) {
            return error.code();
        }
    });
    REQUIRE(query.wait_for(std::chrono::milliseconds{10}) == std::future_status::timeout);

    stopSource.request_stop();

    REQUIRE(query.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    REQUIRE(query.get() == std::make_error_code(std::errc::operation_canceled));

    executeSql(path, "UPDATE slow_query_control SET max_value = 1;");
    REQUIRE(slowRepository.count(request) == 1);

    executeSql(path, "UPDATE slow_query_control SET max_value = 100000000;");
    std::stop_source readStopSource;
    auto read = std::async(std::launch::async, [&] {
        try {
            static_cast<void>(slowRepository.readAll(
                request, [](const ssa::domain::SsaRecord&) { return std::nullopt; },
                readStopSource.get_token()));
            return std::error_code{};
        } catch (const std::system_error& error) {
            return error.code();
        }
    });
    REQUIRE(read.wait_for(std::chrono::milliseconds{10}) == std::future_status::timeout);

    readStopSource.request_stop();

    REQUIRE(read.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    REQUIRE(read.get() == std::make_error_code(std::errc::operation_canceled));

    executeSql(path, "UPDATE slow_query_control SET max_value = 1;");
    REQUIRE(slowRepository.count(request) == 1);
}

TEST_CASE("sqlite maintenance port runs vacuum analyze") {
    const SqliteFixture fixture;
    ssa::infra::sqlite::SqliteMaintenancePort maintenance(fixture.path);

    REQUIRE_FALSE(hasIndex(fixture.path, "idx_ssa_table_derivada_de"));

    const auto result = maintenance.vacuumAnalyze();

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Succeeded);
    REQUIRE(hasIndex(fixture.path, "idx_ssa_table_derivada_de"));
}

TEST_CASE("sqlite maintenance rejects a stopped token before changing data") {
    const SqliteFixture fixture;
    ssa::infra::sqlite::SqliteMaintenancePort maintenance(fixture.path);
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = maintenance.resetDatabase(stopSource.get_token());
    const ssa::infra::sqlite::SqliteSsaRepository repository(fixture.path);

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(result.message.find("canceled") != std::string::npos);
    REQUIRE(repository.count({}) == 4);
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

TEST_CASE("sqlite derivadas rejects a stopped token before changing data") {
    const SqliteFixture fixture;
    ssa::infra::sqlite::SqliteDerivadasPort derivadasPort(fixture.path);
    std::stop_source stopSource;
    stopSource.request_stop();

    const auto result = derivadasPort.syncDerivadas(stopSource.get_token());
    const ssa::infra::sqlite::SqliteSsaRepository repository(fixture.path);
    const auto record = repository.recordBySsaNumber(ssa::domain::SsaNumber{"202500003"});

    REQUIRE(result.status == ssa::ports::WorkflowStatus::Canceled);
    REQUIRE(result.message.find("canceled") != std::string::npos);
    REQUIRE(record.has_value());
    REQUIRE(record->valueOf("derivada_de") == "202400001");
}
