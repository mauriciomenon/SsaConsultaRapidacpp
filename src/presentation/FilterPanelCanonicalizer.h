#pragma once

#include "presentation/FilterPanelState.h"

#include <QString>

namespace ssa::presentation::filterpanel {

    [[nodiscard]] QString executorColumnKey();
    [[nodiscard]] QString statusColumnKey();

    bool removeColumnFiltersShadowedByAdvancedText(FilterPanelState& state);
    bool clearStatusExclusionIfStatusIncludesExcluded(FilterPanelState& state);
    bool foldQuickSectorIntoAdvancedExecutor(FilterPanelState& state);
    bool clearExecutorShortcut(FilterPanelState& state);
    bool setExecutorShortcut(FilterPanelState& state, QString value);
    bool foldLegacyQuickSector(FilterPanelState& state);

} // namespace ssa::presentation::filterpanel
