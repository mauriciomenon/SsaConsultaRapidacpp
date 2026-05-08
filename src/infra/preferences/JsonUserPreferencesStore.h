#pragma once

#include "ports/IUserPreferencesStore.h"

#include <filesystem>

namespace ssa::infra::preferences {

    class JsonUserPreferencesStore final : public ports::IUserPreferencesStore {
      public:
        explicit JsonUserPreferencesStore(std::filesystem::path path);

        [[nodiscard]] ports::UserPreferencesSnapshot load() const override;
        void save(const ports::UserPreferencesSnapshot& snapshot) const override;

      private:
        std::filesystem::path path_;
    };

} // namespace ssa::infra::preferences
