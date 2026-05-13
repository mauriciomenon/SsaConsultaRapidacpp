#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"

#include <QVariant>

#include <string>
#include <string_view>

namespace ssa::presentation {

    class SsaRecordValueFormatter final {
      public:
        [[nodiscard]] QVariant valueFor(const domain::SsaRecord& record,
                                        const std::string& columnKey,
                                        domain::ColumnType columnType) const;
        [[nodiscard]] QVariant valueFor(std::string_view value,
                                        domain::ColumnType columnType) const;
    };

} // namespace ssa::presentation
