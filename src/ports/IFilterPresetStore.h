#pragma once

#include "ports/IUserPreferencesStore.h"

#include <filesystem>
#include <stop_token>

namespace ssa::ports {

    inline constexpr int kCurrentFilterPresetSchemaVersion = 1;

    struct FilterPresetSnapshot {
        FilterPreferencesSnapshot filters;
    };

    class IFilterPresetStore {
      public:
        virtual ~IFilterPresetStore() = default;

        [[nodiscard]] virtual FilterPresetSnapshot load(std::filesystem::path path,
                                                        std::stop_token stopToken = {}) const = 0;
        virtual void save(std::filesystem::path path, const FilterPresetSnapshot& snapshot,
                          std::stop_token stopToken = {}) const = 0;
    };

} // namespace ssa::ports
