#pragma once

#include "ports/IFilterPresetStore.h"

namespace ssa::infra::preferences {

    class JsonFilterPresetStore final : public ports::IFilterPresetStore {
      public:
        [[nodiscard]] ports::FilterPresetSnapshot
        load(std::filesystem::path path, std::stop_token stopToken = {}) const override;
        void save(std::filesystem::path path, const ports::FilterPresetSnapshot& snapshot,
                  std::stop_token stopToken = {}) const override;
    };

} // namespace ssa::infra::preferences
