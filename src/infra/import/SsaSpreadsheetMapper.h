#pragma once

#include "infra/import/SpreadsheetTable.h"

#include <functional>
#include <stop_token>

namespace ssa::infra::importing {

    class SsaSpreadsheetMapper final {
      public:
        using MappingCheckpoint = std::function<void()>;

        [[nodiscard]] static SsaImportBatch map(const SpreadsheetTable& table,
                                                const std::stop_token& stopToken = {});
        [[nodiscard]] static SsaImportBatch map(const SpreadsheetTable& table,
                                                const std::stop_token& stopToken,
                                                const MappingCheckpoint& afterFirstRowMapped);
    };

} // namespace ssa::infra::importing
