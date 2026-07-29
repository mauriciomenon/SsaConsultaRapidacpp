#include "infra/import/DataSetupPort.h"

#include "domain/ColumnCatalog.h"
#include "infra/import/CancelableFileCopy.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "infra/sqlite/SqliteDatabaseValidator.h"
#include "infra/sqlite/SqliteSsaImportWriter.h"
#include "qt/FilesystemPath.h"

#include <QTemporaryDir>

#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace ssa::infra::importing {

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
            std::filesystem::create_directories(inputDirectory / "processadas" / "nosurvivor");

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
                    const auto copy =
                        copyFileAtomically({request.sourceDatabase, stagedDatabasePath}, stopToken);
                    if (!copy.ok()) {
                        const auto canceled =
                            copy.status == FileCopyStatus::Canceled || stopToken.stop_requested();
                        return complete({false,
                                         canceled ? "Configuracao de dados cancelada"
                                                  : "Falha ao copiar o banco de dados",
                                         copy.diagnostic, databasePath});
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
                if (importsXlsx) {
                    SpreadsheetImportWorkflowPort workflow(inputDirectory, stagedDatabasePath,
                                                           columns);
                    ports::ImportExternalFilesRequest importRequest;
                    importRequest.files = request.xlsxFiles;
                    const auto imported = workflow.importExternalFiles(importRequest, stopToken);
                    if (!imported.ok()) {
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

                const auto published = filePublisher_(stagedDatabasePath, databasePath);
                if (published.status != ports::ExclusiveFilePublishStatus::Succeeded) {
                    return complete(
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
