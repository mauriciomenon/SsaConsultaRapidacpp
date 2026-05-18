#pragma once

#include "ports/IFilterPresetStore.h"

namespace ssa::infra::preferences {

    class JsonFilterPresetStore final : public ports::IFilterPresetStore {
      public:
        [[nodiscard]] ports::FilterPresetSnapshot load(std::filesystem::path path) const override;
        void save(std::filesystem::path path,
                  const ports::FilterPresetSnapshot& snapshot) const override;
    };

} // namespace ssa::infra::preferences
