#include "domain/ColumnCatalog.h"
#include "infra/import/DataSetupPort.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "platform/AppPaths.h"
#include "platform/ExclusiveFilePublisher.h"
#include "qt/FilesystemPath.h"

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

    class TemporaryDirectory final {
      public:
        TemporaryDirectory() {
            REQUIRE(directory_.isValid());
        }

        [[nodiscard]] std::filesystem::path path() const {
            return ssa::qt::toFileSystemPath(directory_.path());
        }

      private:
        QTemporaryDir directory_;
    };

    std::string scalarText(sqlite3* database, const std::string_view sql) {
        ssa::infra::sqlite::SqliteStatement statement(database, std::string{sql});
        REQUIRE(statement.step());
        return statement.columnText(0);
    }

    bool hasIndex(sqlite3* database, const std::string_view name) {
        ssa::infra::sqlite::SqliteStatement statement(
            database, "SELECT 1 FROM sqlite_master WHERE type='index' AND name=?");
        statement.bindTextOneBased(1, std::string{name});
        return statement.step();
    }

    void insertSsa(const std::filesystem::path& databasePath, const std::string_view number) {
        ssa::infra::sqlite::SqliteConnection database(
            databasePath, ssa::infra::sqlite::SqliteOpenMode::ReadWrite);
        ssa::infra::sqlite::SqliteStatement statement(
            database.handle(), "INSERT INTO ssa_table (numero_ssa) VALUES (?)");
        statement.bindTextOneBased(1, std::string{number});
        REQUIRE_FALSE(statement.step());
    }

    std::string readBytes(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

    std::vector<std::filesystem::path> regularFilesBelow(const std::filesystem::path& directory) {
        std::vector<std::filesystem::path> files;
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator iterator(directory, error), end;
             !error && iterator != end; iterator.increment(error)) {
            if (iterator->is_regular_file(error)) {
                files.push_back(iterator->path());
            }
            error.clear();
        }
        REQUIRE_FALSE(error);
        return files;
    }

} // namespace

TEST_CASE("DataSetupBootstrap: AppPaths creates data and input folders idempotently") {
    const TemporaryDirectory temporary;
    const auto projectRoot = temporary.path() / "project";
    const ssa::platform::AppPaths paths(QString::fromStdString(projectRoot.string()),
                                        QString::fromStdString((projectRoot / "config").string()));

    REQUIRE_FALSE(std::filesystem::exists(projectRoot / "data"));
    REQUIRE_FALSE(std::filesystem::exists(paths.redundantFolderPath()));

    paths.ensureDataDirectory();
    paths.ensureInputFolders();
    paths.ensureDataDirectory();
    paths.ensureInputFolders();

    REQUIRE(std::filesystem::is_directory(projectRoot / "data"));
    REQUIRE(std::filesystem::is_directory(paths.inputFolderPath()));
    REQUIRE(std::filesystem::is_directory(paths.processedFolderPath()));
    REQUIRE(std::filesystem::is_directory(paths.redundantFolderPath()));
}

TEST_CASE("DataSetupBootstrap: exclusive publisher moves complete bytes to an absent destination") {
    const TemporaryDirectory temporary;
    const auto source = temporary.path() / "staged.db";
    const auto destination = temporary.path() / "published.db";
    {
        std::ofstream output(source, std::ios::binary);
        output << "staged-database-bytes";
        REQUIRE(output.good());
    }

    const auto result = ssa::platform::publishFileExclusively(source, destination);

    REQUIRE(result.status == ssa::ports::ExclusiveFilePublishStatus::Succeeded);
    REQUIRE(result.diagnostic.empty());
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(readBytes(destination) == "staged-database-bytes");
}

TEST_CASE("DataSetupBootstrap: exclusive publisher preserves an existing destination and source") {
    const TemporaryDirectory temporary;
    const auto source = temporary.path() / "staged.db";
    const auto destination = temporary.path() / "published.db";
    {
        std::ofstream output(source, std::ios::binary);
        output << "new-staged-bytes";
        REQUIRE(output.good());
    }
    {
        std::ofstream output(destination, std::ios::binary);
        output << "existing-published-bytes";
        REQUIRE(output.good());
    }

    const auto result = ssa::platform::publishFileExclusively(source, destination);

    REQUIRE(result.status == ssa::ports::ExclusiveFilePublishStatus::DestinationExists);
    REQUIRE(readBytes(source) == "new-staged-bytes");
    REQUIRE(readBytes(destination) == "existing-published-bytes");
}

TEST_CASE("DataSetupBootstrap: exclusive publisher reports a missing source as failure") {
    const TemporaryDirectory temporary;
    const auto source = temporary.path() / "missing.db";
    const auto destination = temporary.path() / "published.db";

    const auto result = ssa::platform::publishFileExclusively(source, destination);

    REQUIRE(result.status == ssa::ports::ExclusiveFilePublishStatus::Failed);
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE_FALSE(std::filesystem::exists(destination));
}

TEST_CASE("DataSetupBootstrap: SqliteSsaImportWriter creates an idempotent empty schema database") {
    const TemporaryDirectory temporary;
    const auto databasePath = temporary.path() / "nested" / "ssas.db";
    const auto columns = ssa::domain::ColumnCatalog::schemaColumns();

    ssa::infra::sqlite::SqliteSsaImportWriter::createEmpty(databasePath, columns);
    ssa::infra::sqlite::SqliteSsaImportWriter::createEmpty(databasePath, columns, "ssa_table",
                                                           std::chrono::milliseconds{50});

    ssa::infra::sqlite::SqliteConnection database(databasePath,
                                                  ssa::infra::sqlite::SqliteOpenMode::ReadOnly);
    REQUIRE(scalarText(database.handle(), "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarText(database.handle(), "PRAGMA user_version") ==
            std::to_string(ssa::domain::ColumnCatalog::schemaVersion()));
    REQUIRE(scalarText(database.handle(),
                       "SELECT name FROM sqlite_master WHERE type='table' AND name='ssa_table'") ==
            "ssa_table");
    REQUIRE(scalarText(database.handle(), "SELECT COUNT(*) FROM ssa_table") == "0");

    for (const auto name : {"ux_ssa_table_numero_ssa", "idx_ssa_table_import_dirty_canonical",
                            "idx_ssa_table_status_last_numero_ssa_desc", "idx_ssa_table_situacao",
                            "idx_ssa_table_setor_executor", "idx_ssa_table_derivada_de",
                            "idx_ssa_table_semana_programada", "idx_ssa_table_semana_executada"}) {
        INFO(name);
        REQUIRE(hasIndex(database.handle(), name));
    }
}

TEST_CASE("DataSetupPort creates the canonical tree and a valid empty database") {
    const TemporaryDirectory temporary;
    const auto projectRoot = temporary.path() / "selected-root";
    const auto databasePath = projectRoot / "data" / "ssas.db";
    ssa::infra::importing::DataSetupPort port(ssa::platform::publishFileExclusively);

    const auto result = port.execute(
        {.action = ssa::ports::DataSetupAction::CreateEmpty, .projectRoot = projectRoot});

    REQUIRE(result.ok);
    REQUIRE(result.databasePath == databasePath);
    for (const auto& directory :
         {projectRoot / "data", projectRoot / "config", projectRoot / "docs_entrada",
          projectRoot / "docs_entrada" / "processadas",
          projectRoot / "docs_entrada" / "processadas" / "nosurvivor"}) {
        INFO(directory.string());
        REQUIRE(std::filesystem::is_directory(directory));
    }
    ssa::infra::sqlite::SqliteConnection database(databasePath,
                                                  ssa::infra::sqlite::SqliteOpenMode::ReadOnly);
    REQUIRE(scalarText(database.handle(), "PRAGMA integrity_check") == "ok");
    REQUIRE(scalarText(database.handle(), "SELECT COUNT(*) FROM ssa_table") == "0");
}

TEST_CASE("DataSetupPort rejects an existing destination without changing its bytes") {
    const TemporaryDirectory temporary;
    const auto projectRoot = temporary.path() / "selected-root";
    const auto databasePath = projectRoot / "data" / "ssas.db";
    std::filesystem::create_directories(databasePath.parent_path());
    {
        std::ofstream output(databasePath, std::ios::binary);
        output << "existing-database-sentinel";
    }
    const auto original = readBytes(databasePath);
    ssa::infra::importing::DataSetupPort port(ssa::platform::publishFileExclusively);

    const auto result = port.execute(
        {.action = ssa::ports::DataSetupAction::CreateEmpty, .projectRoot = projectRoot});

    REQUIRE_FALSE(result.ok);
    REQUIRE(result.message == "O banco de destino ja existe");
    REQUIRE(readBytes(databasePath) == original);
}

TEST_CASE("DataSetupPort copies and validates a populated database") {
    const TemporaryDirectory temporary;
    const auto source = temporary.path() / "source.db";
    const auto projectRoot = temporary.path() / "selected-root";
    const auto destination = projectRoot / "data" / "ssas.db";
    ssa::infra::sqlite::SqliteSsaImportWriter::createEmpty(
        source, ssa::domain::ColumnCatalog::schemaColumns());
    insertSsa(source, "202600001");
    ssa::infra::importing::DataSetupPort port(ssa::platform::publishFileExclusively);

    const auto result = port.execute({.action = ssa::ports::DataSetupAction::ImportDatabase,
                                      .projectRoot = projectRoot,
                                      .sourceDatabase = source});

    REQUIRE(result.ok);
    REQUIRE(result.databasePath == destination);
    REQUIRE(std::filesystem::is_regular_file(source));
    ssa::infra::sqlite::SqliteConnection database(destination,
                                                  ssa::infra::sqlite::SqliteOpenMode::ReadOnly);
    REQUIRE(scalarText(database.handle(), "SELECT COUNT(*) FROM ssa_table") == "1");
    REQUIRE(scalarText(database.handle(), "SELECT numero_ssa FROM ssa_table") == "202600001");
}

TEST_CASE("DataSetupPort reports missing required inputs without leaving a database") {
    const TemporaryDirectory temporary;
    ssa::infra::importing::DataSetupPort port(ssa::platform::publishFileExclusively);

    const auto missingDatabaseRoot = temporary.path() / "missing-database";
    const auto missingDatabase =
        port.execute({.action = ssa::ports::DataSetupAction::ImportDatabase,
                      .projectRoot = missingDatabaseRoot});
    REQUIRE_FALSE(missingDatabase.ok);
    REQUIRE_FALSE(missingDatabase.message.empty());
    REQUIRE_FALSE(std::filesystem::exists(missingDatabaseRoot / "data" / "ssas.db"));

    const auto missingXlsxRoot = temporary.path() / "missing-xlsx";
    const auto missingXlsx = port.execute(
        {.action = ssa::ports::DataSetupAction::ImportXlsx, .projectRoot = missingXlsxRoot});
    REQUIRE_FALSE(missingXlsx.ok);
    REQUIRE_FALSE(missingXlsx.message.empty());
    REQUIRE_FALSE(std::filesystem::exists(missingXlsxRoot / "data" / "ssas.db"));
}

TEST_CASE("DataSetupPort rejects an invalid action without creating a database") {
    const TemporaryDirectory temporary;
    const auto projectRoot = temporary.path() / "invalid-action";
    ssa::infra::importing::DataSetupPort port(ssa::platform::publishFileExclusively);

    const auto result = port.execute(
        {.action = static_cast<ssa::ports::DataSetupAction>(99), .projectRoot = projectRoot});

    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.message.empty());
    REQUIRE_FALSE(std::filesystem::exists(projectRoot / "data" / "ssas.db"));
}

TEST_CASE("DataSetupPort removes only its copied destination after validation failure") {
    const TemporaryDirectory temporary;
    const auto source = temporary.path() / "invalid.db";
    {
        std::ofstream output(source, std::ios::binary);
        output << "not-a-sqlite-database";
    }
    const auto sourceBytes = readBytes(source);
    const auto projectRoot = temporary.path() / "selected-root";
    ssa::infra::importing::DataSetupPort port(ssa::platform::publishFileExclusively);

    const auto result = port.execute({.action = ssa::ports::DataSetupAction::ImportDatabase,
                                      .projectRoot = projectRoot,
                                      .sourceDatabase = source});

    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.diagnostic.empty());
    REQUIRE_FALSE(std::filesystem::exists(projectRoot / "data" / "ssas.db"));
    REQUIRE(readBytes(source) == sourceBytes);
}

TEST_CASE("DataSetupPort leaves no private files after an XLSX setup attempt fails") {
    const TemporaryDirectory temporary;
    const auto source = temporary.path() / "invalid.xlsx";
    {
        std::ofstream output(source, std::ios::binary);
        output << "not-an-xlsx-package";
    }
    const auto sourceBytes = readBytes(source);
    const auto projectRoot = temporary.path() / "selected-root";
    ssa::infra::importing::DataSetupPort port(ssa::platform::publishFileExclusively);

    const auto result = port.execute({.action = ssa::ports::DataSetupAction::ImportXlsx,
                                      .projectRoot = projectRoot,
                                      .xlsxFiles = {source}});

    REQUIRE_FALSE(result.ok);
    REQUIRE(std::filesystem::is_directory(projectRoot / "data"));
    REQUIRE(regularFilesBelow(projectRoot).empty());
    REQUIRE(readBytes(source) == sourceBytes);
}
