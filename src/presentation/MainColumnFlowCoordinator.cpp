#include "presentation/MainColumnFlowCoordinator.h"

#include "presentation/BrowseViewModel.h"
#include "presentation/ColumnSettingsModel.h"

namespace ssa::presentation {

    MainColumnFlowCoordinator::MainColumnFlowCoordinator(BrowseViewModel& browse,
                                                         ColumnSettingsModel& columns,
                                                         const SaveTrigger savePreferences,
                                                         QObject* parent)
        : QObject(parent), browse_(browse), columns_(columns),
          savePreferences_(std::move(savePreferences)) {}

    void MainColumnFlowCoordinator::applyColumnSettings() {
        browse_.applyColumnSettings(columns_.visibleKeys(), columns_.columnWidths());
        if (savePreferences_) {
            savePreferences_();
        }
    }

    void MainColumnFlowCoordinator::resetColumnSettings() {
        columns_.resetDefaults();
    }

    void MainColumnFlowCoordinator::discardColumnSettings() {
        columns_.applyPreferences(browse_.visibleColumns(), browse_.columnWidths());
    }

} // namespace ssa::presentation
