#pragma once

#include "ports/IWorkflowPorts.h"

#include <filesystem>
#include <optional>

namespace ssa::infra::sqlite {

    class SqliteConnection;

    class SqliteMaintenancePort final : public ports::IDatabaseMaintenancePort {
      public:
        explicit SqliteMaintenancePort(std::filesystem::path databasePath);

        [[nodiscard]] ports::WorkflowResult resetDatabase() override;
        [[nodiscard]] ports::WorkflowResult cleanData() override;
        [[nodiscard]] ports::WorkflowResult vacuumAnalyze() override;

      private:
        [[nodiscard]] static std::optional<ports::WorkflowResult>
        executeMaintenanceSql(SqliteConnection& connection, const char* sql);
        [[nodiscard]] static std::optional<ports::WorkflowResult>
        runOptimizationTasks(SqliteConnection& connection);

        std::filesystem::path databasePath_;
    };

} // namespace ssa::infra::sqlite
