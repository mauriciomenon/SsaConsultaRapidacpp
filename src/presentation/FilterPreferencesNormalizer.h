#pragma once

#include "ports/IUserPreferencesStore.h"

#include <vector>

namespace ssa::presentation {

    void normalizeFilterPreferences(ports::FilterPreferencesSnapshot& filters);
    void normalizeSavedFilterPreferences(std::vector<ports::SavedFilterSnapshot>& filters);

} // namespace ssa::presentation
