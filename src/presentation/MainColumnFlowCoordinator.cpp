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
        if (saveAppliedColumns_) {
            saveAppliedColumns_(browse_.visibleColumns(), browse_.columnWidths());
        } else if (savePreferences_) {
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
        if (!columns_.columnWidths().contains(key)) {
            return false;
        }

        auto appliedColumnWidths = browse_.columnWidths();
        const int boundedWidth =
            std::clamp(width, columns_.minColumnWidth(), columns_.maxColumnWidth());
        const auto previousWidth = appliedColumnWidths.find(key);
        if (previousWidth != appliedColumnWidths.end() && previousWidth->second == boundedWidth) {
            return false;
        }

        appliedColumnWidths[key] = boundedWidth;
        browse_.applyColumnSettings(browse_.visibleColumns(), appliedColumnWidths);
        columns_.applyPreferences(browse_.visibleColumns(), browse_.columnWidths());
        if (saveAppliedColumns_) {
            saveAppliedColumns_(browse_.visibleColumns(), browse_.columnWidths());
        }
        return true;
    }

    bool MainColumnFlowCoordinator::setColumnVisibleAndApply(const QString& columnKey,
                                                             const bool visible) {
        const auto key = columnKey.toStdString();
        if (!columns_.columnWidths().contains(key)) {
            return false;
        }

        auto appliedVisibleKeys = browse_.visibleColumns();
        const auto appliedPosition = std::ranges::find(appliedVisibleKeys, key);
        const bool currentlyVisible = appliedPosition != appliedVisibleKeys.end();
        if (currentlyVisible == visible) {
            return false;
        }
        if (!visible && appliedVisibleKeys.size() <= 1) {
            return false;
        }

        if (visible) {
            appliedVisibleKeys.push_back(key);
        } else {
            appliedVisibleKeys.erase(appliedPosition);
        }
        browse_.applyColumnSettings(appliedVisibleKeys, browse_.columnWidths());
        columns_.applyPreferences(browse_.visibleColumns(), browse_.columnWidths());
        if (saveAppliedColumns_) {
            saveAppliedColumns_(browse_.visibleColumns(), browse_.columnWidths());
        }
        return true;
    }

    bool MainColumnFlowCoordinator::canHideColumn(const QString& columnKey) const {
        const auto visibleKeys = browse_.visibleColumns();
        const auto key = columnKey.toStdString();
        return visibleKeys.size() > 1 && std::ranges::find(visibleKeys, key) != visibleKeys.end();
    }

} // namespace ssa::presentation
