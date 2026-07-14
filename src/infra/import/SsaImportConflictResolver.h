#pragma once

#include "infra/SsaImportData.h"

#include <vector>

namespace ssa::infra::importing {

    class SsaImportConflictResolver final {
      public:
        [[nodiscard]] ResolvedSsaImportRows
        resolveBySsaNumberKeepingUnkeyedRows(const std::vector<SsaImportBatch>& batches) const;
    };

} // namespace ssa::infra::importing
