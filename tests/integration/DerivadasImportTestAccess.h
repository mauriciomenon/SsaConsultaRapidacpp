#pragma once

#include "infra/import/DerivadasSourceReader.h"
#include "infra/sqlite/SqliteDerivadasPort.h"

#include <filesystem>
#include <functional>
#include <stop_token>
#include <utility>

namespace ssa::infra::importing {

    class DerivadasImportTestAccess final {
      public:
        [[nodiscard]] static DerivadasSourceResult
        readWithParsingCheckpoint(const std::filesystem::path& source,
                                  const std::stop_token& stopToken,
                                  std::function<void()> afterFirstParsingChunk) {
            return DerivadasSourceReader::read(source, stopToken, afterFirstParsingChunk);
        }

        [[nodiscard]] static ports::WorkflowResult importWithCheckpoints(
            sqlite::SqliteDerivadasPort& port, const ports::ImportDerivationsRequest& request,
            const std::stop_token& stopToken, std::function<void()> afterFirstParsingChunk = {},
            std::function<void()> afterFirstEdgeMerged = {}) {
            return port.importDerivations(
                request, stopToken,
                {.afterFirstParsingChunk = std::move(afterFirstParsingChunk),
                 .afterFirstEdgeMerged = std::move(afterFirstEdgeMerged)});
        }
    };

} // namespace ssa::infra::importing
