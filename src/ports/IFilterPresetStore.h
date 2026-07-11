#pragma once

#include "ports/IUserPreferencesStore.h"

#include <filesystem>

namespace ssa::ports {

    inline constexpr int kCurrentFilterPresetSchemaVersion = 1;

    struct FilterPresetSnapshot {
        FilterPreferencesSnapshot filters;
    };

    class IFilterPresetStore {
      public:
        virtual ~IFilterPresetStore() = default;

        [[nodiscard]] virtual FilterPresetSnapshot load(std::filesystem::path path) const = 0;
        virtual void save(std::filesystem::path path,
                          const FilterPresetSnapshot& snapshot) const = 0;
    };

} // namespace ssa::ports
