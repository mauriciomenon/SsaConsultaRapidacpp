#pragma once

#include "ports/IWorkflowPorts.h"

#include <filesystem>

namespace ssa::infra::sqlite {

    class SqliteDerivadasPort final : public ports::IDerivadasPort {
      public:
        explicit SqliteDerivadasPort(std::filesystem::path databasePath);

        [[nodiscard]] ports::WorkflowResult syncDerivadas() override;

      private:
        std::filesystem::path databasePath_;
    };

} // namespace ssa::infra::sqlite
