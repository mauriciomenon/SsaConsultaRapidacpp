#include "SsaCliController.h"

#include "application/UnavailableWorkflowPort.h"
#include "infra/export/CsvExportPort.h"
#include "infra/sqlite/SqliteMaintenancePort.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "query/SsaQueryService.h"

#include <QCoreApplication>

#include <filesystem>
#include <memory>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ssa_consulta_rapida_cli");
    QCoreApplication::setApplicationVersion("0.1.0");

    const ssa::app::cli::SsaCliController controller(
        [](const std::filesystem::path& dbPath) {
            const auto repository =
                std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(dbPath);
            const auto queryService = std::make_shared<ssa::query::SsaQueryService>(repository);
            return std::make_shared<ssa::application::SsaBrowseService>(queryService);
        },
        [] {
            auto unavailable = std::make_shared<ssa::application::UnavailableWorkflowPort>();
            return std::make_shared<ssa::application::SsaWorkflowService>(unavailable, unavailable,
                                                                          unavailable, unavailable);
        },
        [](const std::filesystem::path& dbPath) {
            const auto repository =
                std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(dbPath);
            const auto exportPort =
                std::make_shared<ssa::infra::exporting::CsvExportPort>(repository);
            const auto maintenancePort =
                std::make_shared<ssa::infra::sqlite::SqliteMaintenancePort>(dbPath);
            auto unavailable = std::make_shared<ssa::application::UnavailableWorkflowPort>();
            return std::make_shared<ssa::application::SsaWorkflowService>(
                unavailable, exportPort, maintenancePort, unavailable);
        });
    return controller.run(QCoreApplication::arguments());
}
