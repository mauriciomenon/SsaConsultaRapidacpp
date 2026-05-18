#pragma once

#include "domain/SsaTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace ssa::domain {

    enum class SsaRelationKind {
        Current,
        DerivedFrom,
        Related,
    };

    struct SsaRelationItem final {
        SsaRelationKind kind{SsaRelationKind::Related};
        std::string number;
    };

    class SsaRelationGraph final {
      public:
        [[nodiscard]] static std::vector<SsaRelationItem> fromRecord(const SsaRecord& record);
    };

} // namespace ssa::domain
