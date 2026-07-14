#pragma once

#include "ports/IWorkflowPorts.h"

#include <filesystem>

namespace ssa::infra::sqlite {

    class SqliteDerivadasPort final : public ports::IDerivadasPort {
      public:
        explicit SqliteDerivadasPort(std::filesystem::path databasePath);

        [[nodiscard]] ports::WorkflowResult
        cleanOrphanDerivations(std::stop_token stopToken = {}) override;

      private:
        std::filesystem::path databasePath_;
    };

} // namespace ssa::infra::sqlite
