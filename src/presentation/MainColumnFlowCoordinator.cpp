#include "presentation/MainColumnFlowCoordinator.h"

#include "presentation/BrowseViewModel.h"
#include "presentation/ColumnSettingsModel.h"

#include <algorithm>

namespace ssa::presentation {

    MainColumnFlowCoordinator::MainColumnFlowCoordinator(
        BrowseViewModel& browse, ColumnSettingsModel& columns, const SaveTrigger savePreferences,
        SaveAppliedColumnsTrigger saveAppliedColumns, QObject* parent)
        : QObject(parent), browse_(browse), columns_(columns),
          savePreferences_(std::move(savePreferences)),
          saveAppliedColumns_(std::move(saveAppliedColumns)) {}

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
        const auto key = columnKey.toStdString();
        auto appliedColumnWidths = browse_.columnWidths();
        const auto previousWidth = appliedColumnWidths.find(key);
        if (!columns_.setColumnWidth(columnKey, width)) {
            return false;
        }

        const auto stagedColumnWidths = columns_.columnWidths();
        const auto stagedWidth = stagedColumnWidths.find(key);
        if (stagedWidth == stagedColumnWidths.end()) {
            return false;
        }
        if (previousWidth != appliedColumnWidths.end() &&
            previousWidth->second == stagedWidth->second) {
            return false;
        }

        appliedColumnWidths[key] = stagedWidth->second;
        browse_.applyColumnSettings(browse_.visibleColumns(), appliedColumnWidths);
        if (saveAppliedColumns_) {
            saveAppliedColumns_(browse_.visibleColumns(), std::move(appliedColumnWidths));
        }
        return true;
    }

    bool MainColumnFlowCoordinator::setColumnVisibleAndApply(const QString& columnKey,
                                                             const bool visible) {
        const auto key = columnKey.toStdString();
        auto appliedVisibleKeys = browse_.visibleColumns();
        const auto appliedPosition = std::ranges::find(appliedVisibleKeys, key);
        const bool currentlyVisible = appliedPosition != appliedVisibleKeys.end();
        if (currentlyVisible == visible) {
            return false;
        }
        if (!visible && (!currentlyVisible || appliedVisibleKeys.size() <= 1)) {
            return false;
        }
        if (!columns_.setColumnVisibleByKey(columnKey, visible)) {
            return false;
        }

        if (visible) {
            appliedVisibleKeys.push_back(key);
        } else {
            appliedVisibleKeys.erase(appliedPosition);
        }
        browse_.applyColumnSettings(appliedVisibleKeys, browse_.columnWidths());
        if (saveAppliedColumns_) {
            saveAppliedColumns_(std::move(appliedVisibleKeys), browse_.columnWidths());
        }
        return true;
    }

    bool MainColumnFlowCoordinator::canHideColumn(const QString& columnKey) const {
        const auto visibleKeys = browse_.visibleColumns();
        const auto key = columnKey.toStdString();
        return visibleKeys.size() > 1 && std::ranges::find(visibleKeys, key) != visibleKeys.end();
    }

} // namespace ssa::presentation
