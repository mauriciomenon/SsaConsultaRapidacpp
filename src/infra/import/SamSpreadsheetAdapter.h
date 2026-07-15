#pragma once

#include "infra/import/SpreadsheetTable.h"
#include "ports/IWorkflowPorts.h"

#include <stop_token>
#include <string>
#include <vector>

namespace ssa::infra::importing {

    struct SamSpreadsheetAdaptResult {
        SpreadsheetTable table;
        std::string rejectionReason;

        [[nodiscard]] bool ok() const {
            return rejectionReason.empty();
        }
    };

    class SamSpreadsheetAdapter final {
      public:
        [[nodiscard]] static SamSpreadsheetAdaptResult
        adapt(const std::vector<SpreadsheetTable>& sheets, const ports::SamArtifact& artifact,
              const std::stop_token& stopToken = {});
    };

} // namespace ssa::infra::importing
