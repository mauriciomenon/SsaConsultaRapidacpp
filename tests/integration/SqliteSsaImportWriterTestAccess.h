#pragma once

#include "infra/sqlite/SqliteSsaImportWriter.h"

namespace ssa::infra::sqlite {

    class SqliteSsaImportWriterTestAccess final {
      public:
        [[nodiscard]] static SqliteSsaImportWriterAccess access() {
            return {};
        }
    };

} // namespace ssa::infra::sqlite
