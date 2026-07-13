#pragma once

#include "ports/IWorkflowPorts.h"

#include <atomic>
#include <filesystem>
#include <optional>

namespace ssa::infra::sqlite {

    class SqliteConnection;

    class SqliteMaintenancePort final : public ports::IDatabaseMaintenancePort {
      public:
        explicit SqliteMaintenancePort(std::filesystem::path databasePath);

        [[nodiscard]] ports::WorkflowResult resetDatabase(std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult cleanData(std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult vacuumAnalyze(std::stop_token stopToken = {}) override;

      private:
        [[nodiscard]] ports::WorkflowResult
        runCommittedMaintenance(const char* mutationSql, const std::stop_token& stopToken,
                                const char* successMessage);
        [[nodiscard]] static std::optional<ports::WorkflowResult>
        executeMaintenanceSql(SqliteConnection& connection, const char* sql,
                              const std::atomic_bool* busyCancellationObserved = nullptr);
        [[nodiscard]] static std::optional<ports::WorkflowResult>
        runOptimizationTasks(SqliteConnection& connection,
                             const std::atomic_bool* busyCancellationObserved = nullptr);

        std::filesystem::path databasePath_;
    };

} // namespace ssa::infra::sqlite
