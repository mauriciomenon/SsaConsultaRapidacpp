#include "application/FilterPresetService.h"

namespace ssa::application {

    ports::FilterPresetSnapshot FilterPresetService::createPresetWithClearedSearch(
        const ports::UserPreferencesSnapshot& preferences) const {
        ports::FilterPresetSnapshot preset;
        preset.filters = preferences.filters;
        preset.filters.searchText.clear();
        return preset;
    }

    void FilterPresetService::applyPresetPreservingSearch(
        const ports::FilterPresetSnapshot& preset,
        ports::UserPreferencesSnapshot& preferences) const {
        const auto currentSearchText = preferences.filters.searchText;
        preferences.filters = preset.filters;
        preferences.filters.searchText = currentSearchText;
    }

} // namespace ssa::application
