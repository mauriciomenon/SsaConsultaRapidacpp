#pragma once

#include "ports/IWorkflowPorts.h"

#include <filesystem>
#include <memory>

namespace ssa::infra::importing {
    class LegacySpreadsheetConverter;
}

namespace ssa::infra::sqlite {

    class SqliteDerivadasPort final : public ports::IDerivadasPort {
      public:
        explicit SqliteDerivadasPort(std::filesystem::path databasePath);
        SqliteDerivadasPort(std::filesystem::path databasePath,
                            std::shared_ptr<importing::LegacySpreadsheetConverter> legacyConverter);

        [[nodiscard]] bool legacySpreadsheetConverterAvailable() const override;
        [[nodiscard]] ports::WorkflowResult
        importDerivations(const ports::ImportDerivationsRequest& request,
                          std::stop_token stopToken = {}) override;

        [[nodiscard]] ports::WorkflowResult
        cleanOrphanDerivations(std::stop_token stopToken = {}) override;

      private:
        std::filesystem::path databasePath_;
        std::shared_ptr<importing::LegacySpreadsheetConverter> legacyConverter_;
    };

} // namespace ssa::infra::sqlite
