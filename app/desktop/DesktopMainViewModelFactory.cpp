#include "DesktopMainViewModelFactory.h"

#include "application/SsaBrowseService.h"
#include "application/SsaWorkflowService.h"
#include "domain/ColumnCatalog.h"
#include "infra/export/CsvExportPort.h"
#include "infra/import/LegacySpreadsheetConverter.h"
#include "infra/import/SpreadsheetImportWorkflowPort.h"
#include "infra/preferences/JsonFilterPresetStore.h"
#include "infra/preferences/JsonUserPreferencesStore.h"
#include "infra/sqlite/SqliteDatabaseValidator.h"
#include "infra/sqlite/SqliteDerivadasPort.h"
#include "infra/sqlite/SqliteMaintenancePort.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "platform/DesktopApplicationLauncher.h"
#include "platform/DesktopExternalCommandPort.h"
#include "platform/ScrapReportSamRefreshPort.h"
#include "platform/SupervisedProcessRunner.h"
#include "qt/FilesystemPath.h"
#include "query/SsaQueryService.h"

#include <QUrl>

#include <filesystem>
#include <memory>
#include <vector>

namespace ssa::app::desktop {

    namespace {

        std::filesystem::path databasePath(const ssa::platform::StartupOptions& options) {
            return ssa::qt::toFileSystemPath(options.databasePath);
        }

        std::vector<ssa::domain::ColumnDef> importColumns() {
            return ssa::domain::ColumnCatalog::schemaColumns();
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
            const auto processRunner = std::make_shared<ssa::platform::SupervisedProcessRunner>();
            const auto legacyConverter =
                std::make_shared<ssa::infra::importing::LegacySpreadsheetConverter>(
                    std::filesystem::path{}, processRunner);
            const auto derivadasPort = std::make_shared<ssa::infra::sqlite::SqliteDerivadasPort>(
                databasePath(options), legacyConverter);
            const auto samRefreshPort =
                std::make_shared<ssa::platform::ScrapReportSamRefreshPort>();
            return std::make_shared<ssa::application::SsaWorkflowService>(
                importPort, exportPort, maintenancePort, derivadasPort, samRefreshPort, importPort);
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
        const auto queryService = std::make_shared<ssa::query::SsaQueryService>(repository);
        const auto browseService =
            std::make_shared<ssa::application::SsaBrowseService>(queryService);
        const auto workflows = createWorkflowService(options, paths, repository);
        const auto commands = createCommandPort(options, paths);
        const auto preferences =
            std::make_shared<ssa::infra::preferences::JsonUserPreferencesStore>(
                ssa::qt::toFileSystemPath(paths.preferencesFile()));
        const auto filterPresets =
            std::make_shared<ssa::infra::preferences::JsonFilterPresetStore>();
        const auto databaseValidator =
            std::make_shared<ssa::infra::sqlite::SqliteDatabaseValidator>();
        const auto applicationLauncher =
            std::make_shared<ssa::platform::DesktopApplicationLauncher>(options);

        return std::make_unique<ssa::presentation::MainViewModel>(
            browseService, commands, preferences, filterPresets, workflows, databaseValidator,
            applicationLauncher, queryService);
    }

} // namespace ssa::app::desktop
