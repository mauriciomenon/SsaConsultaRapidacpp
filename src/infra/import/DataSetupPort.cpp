#include "infra/import/DataSetupPort.h"

#include "domain/ColumnCatalog.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "infra/sqlite/SqliteConnection.h"
#include "infra/sqlite/SqliteDatabaseBackup.h"
#include "infra/sqlite/SqliteDatabaseValidator.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "ports/OperationError.h"
#include "qt/FilesystemPath.h"

#include <QTemporaryDir>

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ssa::infra::importing {

    namespace {

        // As the consolidator does, a symlinked folder below the input root (processadas,
        // nosurvivor) is never followed out of the project tree.
        std::optional<std::filesystem::path> symlinkedFolderBelow(const std::filesystem::path& root,
                                                                  std::filesystem::path directory) {
            for (; directory != root && directory != directory.parent_path();
                 directory = directory.parent_path()) {
                if (std::filesystem::is_symlink(std::filesystem::symlink_status(directory))) {
                    return directory;
                }
            }
            return std::nullopt;
        }

        // Moves each file of the private input tree to the same place in the project input
        // tree, never over an existing file; `moved` lists what reached the project tree.
        std::optional<std::string> moveInputCopies(ports::ExclusiveFilePublisher& publisher,
                                                   const std::filesystem::path& from,
                                                   const std::filesystem::path& to,
                                                   std::vector<std::filesystem::path>& moved) {
            try {
                if (!std::filesystem::exists(from)) {
                    return std::nullopt;
                }
                std::vector<std::filesystem::path> copies;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(from)) {
                    if (entry.is_regular_file()) {
                        copies.push_back(entry.path());
                    }
                }
                for (const auto& copy : copies) {
                    const auto target = to / copy.lexically_relative(from);
                    if (const auto link = symlinkedFolderBelow(to, target.parent_path())) {
                        return qt::toUtf8(*link) + ": the folder is a symlink";
                    }
                    std::filesystem::create_directories(target.parent_path());
                    const auto published = publisher(copy, target);
                    if (published.status != ports::ExclusiveFilePublishStatus::Succeeded) {
                        return qt::toUtf8(target) + ": " +
                               (published.status ==
                                        ports::ExclusiveFilePublishStatus::DestinationExists
                                    ? std::string{"a file with this name already exists"}
                                    : published.diagnostic);
                    }
                    moved.push_back(target);
                }
                return std::nullopt;
            } catch (const std::filesystem::filesystem_error& error) {
                return error.what();
            }
        }

        void removeMovedCopies(const std::vector<std::filesystem::path>& moved,
                               std::string& diagnostic) {
            for (const auto& path : moved) {
                std::error_code error;
                std::filesystem::remove(path, error);
                if (error) {
                    diagnostic += (diagnostic.empty() ? "" : "; ") + std::string{"cannot remove "} +
                                  qt::toUtf8(path) + ": " + error.message();
                }
            }
        }

    } // namespace

    DataSetupPort::DataSetupPort(ports::ExclusiveFilePublisher& filePublisher)
        : filePublisher_(filePublisher) {}

    ports::DataSetupResult DataSetupPort::execute(const ports::DataSetupRequest& request,
                                                  const std::stop_token stopToken) {
        if (request.projectRoot.empty()) {
            return {false, "A pasta raiz do projeto nao foi informada", {}, {}};
        }
        bool importsDatabase = false;
        bool importsXlsx = false;
        switch (request.action) {
        case ports::DataSetupAction::CreateEmpty:
            break;
        case ports::DataSetupAction::ImportDatabase:
            importsDatabase = true;
            break;
        case ports::DataSetupAction::ImportXlsx:
            importsXlsx = true;
            break;
        case ports::DataSetupAction::ImportDatabaseAndXlsx:
            importsDatabase = true;
            importsXlsx = true;
            break;
        default:
            return {false, "A acao de configuracao de dados nao e valida", {}, {}};
        }
        if (importsDatabase && request.sourceDatabase.empty()) {
            return {false, "Selecione um banco de dados de origem", {}, {}};
        }
        if (importsXlsx && request.xlsxFiles.empty()) {
            return {false, "Selecione ao menos uma planilha XLSX", {}, {}};
        }
        if (stopToken.stop_requested()) {
            return {false, "Configuracao de dados cancelada", {}, {}};
        }

        const auto dataDirectory = request.projectRoot / "data";
        const auto inputDirectory = request.projectRoot / "docs_entrada";
        const auto databasePath = dataDirectory / "ssas.db";

        try {
            std::filesystem::create_directories(dataDirectory);
            std::filesystem::create_directories(request.projectRoot / "config");
            // Every later rescan refuses a symlinked processadas, so the setup does too.
            const auto noSurvivorDirectory = inputDirectory / "processadas" / "nosurvivor";
            if (const auto link = symlinkedFolderBelow(inputDirectory, noSurvivorDirectory)) {
                return {false, "A pasta docs_entrada contem um atalho (symlink)",
                        qt::toUtf8(*link) + ": the folder is a symlink", databasePath};
            }
            std::filesystem::create_directories(noSurvivorDirectory);

            std::error_code existenceError;
            const bool destinationExists = std::filesystem::exists(databasePath, existenceError);
            if (existenceError) {
                return {false, "Falha ao verificar o banco de destino", existenceError.message(),
                        databasePath};
            }
            if (destinationExists) {
                return {false, "O banco de destino ja existe", {}, databasePath};
            }

            const auto temporaryTemplate = databasePath.parent_path() / ".ssa-data-setup-XXXXXX";
            QTemporaryDir temporaryDirectory(qt::toQString(temporaryTemplate));
            if (!temporaryDirectory.isValid()) {
                return {false, "Falha ao criar a area temporaria de configuracao",
                        temporaryDirectory.errorString().toStdString(), databasePath};
            }
            const auto temporaryPath = qt::toFileSystemPath(temporaryDirectory.path());
            temporaryDirectory.setAutoRemove(false);
            const auto stagedDatabasePath = temporaryPath / "ssas.db";
            const auto complete = [&temporaryPath](ports::DataSetupResult result) {
                std::error_code cleanupError;
                std::filesystem::remove_all(temporaryPath, cleanupError);
                if (cleanupError) {
                    if (!result.diagnostic.empty()) {
                        result.diagnostic += "; ";
                    }
                    result.diagnostic +=
                        "cannot remove private data setup directory: " + cleanupError.message();
                }
                return result;
            };

            try {
                const auto columns = domain::ColumnCatalog::schemaColumns();
                if (importsDatabase) {
                    // SQLite backup, not a byte copy: committed pages may still be in the
                    // source -wal file while another app keeps the database open.
                    try {
                        sqlite::backupSqliteDatabase(request.sourceDatabase, stagedDatabasePath,
                                                     stopToken, std::chrono::milliseconds{3000},
                                                     {"Falha ao copiar o banco de dados",
                                                      "sqlite backup data setup",
                                                      "sqlite backup data setup init"});
                        // The backup copies a WAL header; the private copy is used alone, so
                        // return it to a rollback journal before validation and publication.
                        const sqlite::SqliteConnection staged(stagedDatabasePath,
                                                              sqlite::SqliteOpenMode::ReadWrite);
                        sqlite::SqliteStatement journalMode(staged.handle(),
                                                            "PRAGMA journal_mode=DELETE");
                        static_cast<void>(journalMode.step());
                    } catch (const std::system_error& error) {
                        const bool canceled =
                            error.code() == std::make_error_code(std::errc::operation_canceled);
                        return complete({false,
                                         canceled ? "Configuracao de dados cancelada"
                                                  : "Falha ao copiar o banco de dados",
                                         error.what(), databasePath});
                    } catch (const ports::OperationError& error) {
                        return complete({false, error.what(), error.diagnostic(), databasePath});
                    }

                    const sqlite::SqliteDatabaseValidator validator;
                    const auto validation = validator.validate(stagedDatabasePath, stopToken);
                    if (!validation.valid()) {
                        const auto canceled =
                            validation.status == ports::DatabaseValidationStatus::Canceled ||
                            stopToken.stop_requested();
                        return complete({false,
                                         canceled ? "Configuracao de dados cancelada"
                                                  : "O banco de dados copiado nao e valido",
                                         validation.message + (validation.diagnostic.empty()
                                                                   ? std::string{}
                                                                   : "; " + validation.diagnostic),
                                         databasePath});
                    }
                } else {
                    sqlite::SqliteSsaImportWriter::createEmpty(stagedDatabasePath, columns);
                }

                if (stopToken.stop_requested()) {
                    return complete({false, "Configuracao de dados cancelada", {}, databasePath});
                }
                // Initial setup is all-or-nothing: the import runs in the private area, so a
                // rejected workbook fails the setup and leaves the project tree untouched.
                const auto stagedInputDirectory = temporaryPath / "docs_entrada";
                if (importsXlsx) {
                    SpreadsheetImportWorkflowPort workflow(stagedInputDirectory, stagedDatabasePath,
                                                           columns);
                    ports::ImportExternalFilesRequest importRequest;
                    importRequest.files = request.xlsxFiles;
                    const auto imported = workflow.importExternalFiles(importRequest, stopToken);
                    if (!imported.ok() || imported.warning) {
                        const auto canceled = imported.status == ports::WorkflowStatus::Canceled ||
                                              stopToken.stop_requested();
                        auto diagnostic = imported.message;
                        if (!imported.diagnostic.empty()) {
                            if (!diagnostic.empty()) {
                                diagnostic += "; ";
                            }
                            diagnostic += imported.diagnostic;
                        }
                        return complete({false,
                                         canceled ? "Configuracao de dados cancelada"
                                                  : "Falha ao importar as planilhas selecionadas",
                                         std::move(diagnostic), databasePath});
                    }
                }

                if (stopToken.stop_requested()) {
                    return complete({false, "Configuracao de dados cancelada", {}, databasePath});
                }

                // The processed copies reach the project tree only together with the database.
                std::vector<std::filesystem::path> movedCopies;
                const auto failWithoutCopies = [&](ports::DataSetupResult result) {
                    removeMovedCopies(movedCopies, result.diagnostic);
                    return complete(std::move(result));
                };
                if (const auto moveError = moveInputCopies(filePublisher_, stagedInputDirectory,
                                                           inputDirectory, movedCopies)) {
                    return failWithoutCopies({false, "Falha ao mover as planilhas processadas",
                                              *moveError, databasePath});
                }
                const auto published = filePublisher_(stagedDatabasePath, databasePath);
                if (published.status != ports::ExclusiveFilePublishStatus::Succeeded) {
                    return failWithoutCopies(
                        {false,
                         published.status == ports::ExclusiveFilePublishStatus::DestinationExists
                             ? "O banco de destino ja existe"
                             : "Falha ao publicar o banco de dados",
                         published.diagnostic, databasePath});
                }

                return complete({true, "Configuracao de dados concluida", {}, databasePath});
            } catch (const std::exception& exception) {
                return complete({false,
                                 stopToken.stop_requested() ? "Configuracao de dados cancelada"
                                                            : "Falha ao configurar os dados",
                                 exception.what(), databasePath});
            }
        } catch (const std::exception& exception) {
            return {false,
                    stopToken.stop_requested() ? "Configuracao de dados cancelada"
                                               : "Falha ao configurar os dados",
                    exception.what(), databasePath};
        }
    }

} // namespace ssa::infra::importing
