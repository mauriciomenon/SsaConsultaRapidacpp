#include "CliServiceFactory.h"

#include "application/UnavailableWorkflowPort.h"
#include "domain/ColumnCatalog.h"
#include "infra/export/CsvExportPort.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "infra/sqlite/SqliteMaintenancePort.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "query/SsaQueryService.h"

namespace ssa::app::cli {

    namespace {

        std::vector<ssa::domain::ColumnDef> importColumns() {
            const auto columns = ssa::domain::ColumnCatalog::all();
            return {columns.begin(), columns.end()};
        }

    } // namespace

    std::shared_ptr<ssa::infra::sqlite::SqliteSsaRepository>
    CliServiceFactory::repositoryForPath(const std::filesystem::path& dbPath) const {
        return std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(dbPath);
    }

    SsaCliController CliServiceFactory::createController() {
        return SsaCliController{
            [this](const std::filesystem::path& dbPath) {
                const auto sqliteRepository = repositoryForPath(dbPath);
                const auto queryService =
                    std::make_shared<ssa::query::SsaQueryService>(sqliteRepository);
                return std::make_shared<ssa::application::SsaBrowseService>(queryService);
            },
            [] {
                auto unavailable = std::make_shared<ssa::application::UnavailableWorkflowPort>();
                return std::make_shared<ssa::application::SsaWorkflowService>(
                    unavailable, unavailable, unavailable, unavailable);
            },
            [this](const std::filesystem::path& dbPath, const std::filesystem::path& inputFolder) {
                const auto sqliteRepository = repositoryForPath(dbPath);
                const auto exportPort =
                    std::make_shared<ssa::infra::exporting::CsvExportPort>(sqliteRepository);
                const auto maintenancePort =
                    std::make_shared<ssa::infra::sqlite::SqliteMaintenancePort>(dbPath);
                const auto importPort =
                    std::make_shared<ssa::infra::importing::SpreadsheetImportWorkflowPort>(
                        inputFolder, dbPath, importColumns());
                auto unavailable = std::make_shared<ssa::application::UnavailableWorkflowPort>();
                return std::make_shared<ssa::application::SsaWorkflowService>(
                    importPort, exportPort, maintenancePort, unavailable);
            }};
    }

} // namespace ssa::app::cli
