#include "DesktopMainViewModelFactory.h"

#include "application/SsaWorkflowService.h"
#include "application/UnavailableWorkflowPort.h"
#include "infra/export/CsvExportPort.h"
#include "infra/preferences/JsonFilterPresetStore.h"
#include "infra/preferences/JsonUserPreferencesStore.h"
#include "infra/sqlite/SqliteSsaRepository.h"
#include "platform/DesktopExternalCommandPort.h"
#include "query/SsaQueryService.h"

#include <QUrl>

#include <filesystem>
#include <memory>
#include <vector>

namespace ssa::app::desktop {

    std::unique_ptr<ssa::presentation::MainViewModel>
    DesktopMainViewModelFactory::create(const ssa::platform::StartupOptions& options,
                                        const ssa::platform::AppPaths& paths) {
        const auto repository = std::make_shared<ssa::infra::sqlite::SqliteSsaRepository>(
            std::filesystem::path{options.databasePath.toStdString()});
        const auto service = std::make_shared<ssa::query::SsaQueryService>(repository);
        const auto exportPort = std::make_shared<ssa::infra::exporting::CsvExportPort>(repository);
        const auto unavailableImportPort =
            std::make_shared<ssa::application::UnavailableWorkflowPort>();
        const auto unavailableMaintenancePort =
            std::make_shared<ssa::application::UnavailableWorkflowPort>();
        const auto unavailableDerivadasPort =
            std::make_shared<ssa::application::UnavailableWorkflowPort>();
        const auto workflows = std::make_shared<ssa::application::SsaWorkflowService>(
            unavailableImportPort, exportPort, unavailableMaintenancePort,
            unavailableDerivadasPort);

        ssa::platform::LocalOpenPaths commandPaths;
        commandPaths.inputFolder = paths.inputFolderPath();
        commandPaths.processedFolder = paths.processedFolderPath();
        commandPaths.redundantFolder = paths.redundantFolderPath();
        commandPaths.installationGuide = paths.installationGuidePath();
        const auto commands = std::make_shared<ssa::platform::DesktopExternalCommandPort>(
            QUrl{options.samBaseUrl}, commandPaths,
            std::vector<std::filesystem::path>{paths.projectRootPath()});
        const auto preferences =
            std::make_shared<ssa::infra::preferences::JsonUserPreferencesStore>(
                std::filesystem::path{paths.preferencesFile().toStdString()});
        const auto filterPresets =
            std::make_shared<ssa::infra::preferences::JsonFilterPresetStore>();

        return std::make_unique<ssa::presentation::MainViewModel>(service, commands, preferences,
                                                                  filterPresets, workflows);
    }

} // namespace ssa::app::desktop
