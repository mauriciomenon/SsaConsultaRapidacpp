#pragma once

#include "infra/import/SpreadsheetTable.h"

namespace ssa::infra::importing {

    class SsaSpreadsheetMapper final {
      public:
        [[nodiscard]] SsaImportBatch map(const SpreadsheetTable& table) const;
    };

} // namespace ssa::infra::importing
