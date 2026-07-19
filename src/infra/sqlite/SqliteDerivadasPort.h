#pragma once

#include "infra/sqlite/SqliteProgressHandler.h"
#include "ports/IWorkflowPorts.h"

#include <filesystem>
#include <functional>
#include <memory>

namespace ssa::infra::importing {
    class DerivadasImportTestAccess;
    struct DerivadasSourceResult;
    class LegacySpreadsheetConverter;
} // namespace ssa::infra::importing

namespace ssa::infra::sqlite {

    class SqliteDerivadasPort final : public ports::IDerivadasPort {
      public:
        using SynchronizationSemaphore = SqliteSynchronizationSemaphore;

        struct SynchronizationSignals {
            std::shared_ptr<SynchronizationSemaphore> busyEntered;
        };

        SqliteDerivadasPort(std::filesystem::path databasePath,
                            std::shared_ptr<importing::LegacySpreadsheetConverter> legacyConverter,
                            SynchronizationSignals synchronization = {});

        [[nodiscard]] bool legacySpreadsheetConverterAvailable() const override;
        [[nodiscard]] ports::WorkflowResult
        importDerivations(const ports::ImportDerivationsRequest& request,
                          std::stop_token stopToken = {}) override;

        [[nodiscard]] ports::WorkflowResult
        cleanOrphanDerivations(std::stop_token stopToken = {}) override;

      private:
        friend class importing::DerivadasImportTestAccess;

        using TestCheckpoint = std::function<void()>;

        struct TestCheckpoints {
            TestCheckpoint afterFirstParsingChunk;
            TestCheckpoint afterFirstEdgeMerged;
        };

        [[nodiscard]] static importing::DerivadasSourceResult
        readLegacySource(const std::filesystem::path& source,
                         const importing::LegacySpreadsheetConverter& converter,
                         const std::stop_token& stopToken,
                         const TestCheckpoint& afterFirstParsingChunk);
        [[nodiscard]] static importing::DerivadasSourceResult
        readSource(const std::filesystem::path& source,
                   const importing::LegacySpreadsheetConverter& converter,
                   const std::stop_token& stopToken, const TestCheckpoint& afterFirstParsingChunk);
        [[nodiscard]] ports::WorkflowResult
        importDerivations(const ports::ImportDerivationsRequest& request,
                          const std::stop_token& stopToken, const TestCheckpoints& checkpoints);

        std::filesystem::path databasePath_;
        std::shared_ptr<importing::LegacySpreadsheetConverter> legacyConverter_;
        SynchronizationSignals synchronization_;
    };

} // namespace ssa::infra::sqlite
