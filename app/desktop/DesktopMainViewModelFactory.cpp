#include "DesktopMainViewModelFactory.h"

#include "application/SsaWorkflowService.h"
#include "domain/ColumnCatalog.h"
#include "infra/export/CsvExportPort.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "infra/preferences/JsonFilterPresetStore.h"
#include "infra/preferences/JsonUserPreferencesStore.h"
#include "infra/sqlite/SqliteDerivadasPort.h"
#include "infra/sqlite/SqliteMaintenancePort.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "platform/DesktopExternalCommandPort.h"
#include "query/SsaQueryService.h"

#include <QUrl>

#include <filesystem>
#include <memory>
#include <vector>

namespace ssa::app::desktop {

    namespace {

        std::filesystem::path toFileSystemPath(const QString& path) {
            return std::filesystem::path{path.toStdString()};
        }

        std::filesystem::path databasePath(const ssa::platform::StartupOptions& options) {
            return toFileSystemPath(options.databasePath);
        }

        std::vector<ssa::domain::ColumnDef> importColumns() {
            return ssa::domain::ColumnCatalog::storageColumns();
        }

        std::shared_ptr<ssa::ports::ISsaRepository>
        createRepository(const ssa::platform::StartupOptions& options) {
            return std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(databasePath(options));
        }

        std::shared_ptr<ssa::application::SsaWorkflowService>
        createWorkflowService(const ssa::platform::StartupOptions& options,
                              const ssa::platform::AppPaths& paths,
                              const std::shared_ptr<ssa::ports::ISsaRepository>& repository) {
            const auto exportPort =
                std::make_shared<ssa::infra::exporting::CsvExportPort>(repository);
            const auto importPort =
                std::make_shared<ssa::infra::importing::SpreadsheetImportWorkflowPort>(
                    paths.inputFolderPath(), databasePath(options), importColumns());
            const auto maintenancePort =
                std::make_shared<ssa::infra::sqlite::SqliteMaintenancePort>(databasePath(options));
            const auto derivadasPort =
                std::make_shared<ssa::infra::sqlite::SqliteDerivadasPort>(databasePath(options));
            return std::make_shared<ssa::application::SsaWorkflowService>(
                importPort, exportPort, maintenancePort, derivadasPort);
        }

        std::shared_ptr<ssa::platform::DesktopExternalCommandPort>
        createCommandPort(const ssa::platform::StartupOptions& options,
                          const ssa::platform::AppPaths& paths) {
            ssa::platform::LocalOpenPaths commandPaths;
            commandPaths.inputFolder = paths.inputFolderPath();
            commandPaths.processedFolder = paths.processedFolderPath();
            commandPaths.redundantFolder = paths.redundantFolderPath();
            commandPaths.installationGuide = paths.installationGuidePath();
            return std::make_shared<ssa::platform::DesktopExternalCommandPort>(
                QUrl{options.samBaseUrl}, commandPaths,
                std::vector<std::filesystem::path>{paths.projectRootPath()});
        }

    } // namespace

    std::unique_ptr<ssa::presentation::MainViewModel>
    DesktopMainViewModelFactory::create(const ssa::platform::StartupOptions& options,
                                        const ssa::platform::AppPaths& paths) {
        const auto repository = createRepository(options);
        const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
        const auto workflows = createWorkflowService(options, paths, repository);
        const auto commands = createCommandPort(options, paths);
        const auto preferences =
            std::make_shared<ssa::infra::preferences::JsonUserPreferencesStore>(
                toFileSystemPath(paths.preferencesFile()));
        const auto filterPresets =
            std::make_shared<ssa::infra::preferences::JsonFilterPresetStore>();

        return std::make_unique<ssa::presentation::MainViewModel>(service, commands, preferences,
                                                                  filterPresets, workflows);
    }

} // namespace ssa::app::desktop
