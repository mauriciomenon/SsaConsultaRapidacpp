#pragma once

#include "infra/import/SpreadsheetTable.h"

namespace ssa::infra::importing {

    class SsaSpreadsheetMapper final {
      public:
        [[nodiscard]] static SsaImportBatch map(const SpreadsheetTable& table);
    };

} // namespace ssa::infra::importing
