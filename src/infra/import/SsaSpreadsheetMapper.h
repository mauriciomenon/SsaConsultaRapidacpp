#pragma once

#include "infra/import/SpreadsheetTable.h"

#include <stop_token>

namespace ssa::infra::importing {

    class SsaSpreadsheetMapper final {
      public:
        [[nodiscard]] static SsaImportBatch map(const SpreadsheetTable& table,
                                                const std::stop_token& stopToken = {});
    };

} // namespace ssa::infra::importing
