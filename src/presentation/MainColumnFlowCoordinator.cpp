#include "presentation/MainColumnFlowCoordinator.h"

#include "presentation/BrowseViewModel.h"
#include "presentation/ColumnSettingsModel.h"

#include <algorithm>

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

    bool MainColumnFlowCoordinator::setColumnWidthAndApply(const QString& columnKey,
                                                           const int width) {
        if (!columns_.setColumnWidth(columnKey, width)) {
            return false;
        }
        applyColumnSettings();
        return true;
    }

    bool MainColumnFlowCoordinator::setColumnVisibleAndApply(const QString& columnKey,
                                                             const bool visible) {
        const auto previousVisibleKeys = columns_.visibleKeys();
        if (!columns_.setColumnVisibleByKey(columnKey, visible)) {
            return false;
        }
        if (columns_.visibleKeys() == previousVisibleKeys) {
            return false;
        }
        applyColumnSettings();
        return true;
    }

    bool MainColumnFlowCoordinator::canHideColumn(const QString& columnKey) const {
        const auto visibleKeys = columns_.visibleKeys();
        const auto key = columnKey.toStdString();
        return visibleKeys.size() > 1 && std::ranges::find(visibleKeys, key) != visibleKeys.end();
    }

} // namespace ssa::presentation
