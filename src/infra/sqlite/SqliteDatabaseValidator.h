#pragma once

#include "ports/IDatabaseSwitchPorts.h"

namespace ssa::infra::sqlite {

    class SqliteDatabaseValidator final : public ports::IDatabaseValidator {
      public:
        [[nodiscard]] ports::DatabaseValidationResult
        validate(const std::filesystem::path& path, std::stop_token stopToken = {}) const override;
    };

} // namespace ssa::infra::sqlite
