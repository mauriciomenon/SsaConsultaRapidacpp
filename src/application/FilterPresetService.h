#pragma once

#include "ports/IFilterPresetStore.h"

namespace ssa::application {

    class FilterPresetService final {
      public:
        [[nodiscard]] ports::FilterPresetSnapshot
        createPresetWithClearedSearch(const ports::UserPreferencesSnapshot& preferences) const;
        void applyPresetPreservingSearch(const ports::FilterPresetSnapshot& preset,
                                         ports::UserPreferencesSnapshot& preferences) const;
    };

} // namespace ssa::application
