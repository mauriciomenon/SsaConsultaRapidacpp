#pragma once

#include "ports/IUserPreferencesStore.h"

#include <filesystem>

namespace ssa::infra::preferences {

    class JsonUserPreferencesStore final : public ports::IUserPreferencesStore {
      public:
        explicit JsonUserPreferencesStore(std::filesystem::path path);

        [[nodiscard]] ports::UserPreferencesSnapshot
        load(std::stop_token stopToken = {}) const override;
        void save(const ports::UserPreferencesSnapshot& snapshot,
                  std::stop_token stopToken = {}) const override;

      private:
        std::filesystem::path path_;
    };

} // namespace ssa::infra::preferences
