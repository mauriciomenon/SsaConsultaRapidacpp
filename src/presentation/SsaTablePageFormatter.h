#pragma once

#include "domain/SsaTypes.h"
#include "presentation/SsaColumnDisplayCatalog.h"
#include "presentation/SsaTableDisplayCache.h"

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace ssa::presentation {

    class SsaTablePageFormatter final {
      public:
        [[nodiscard]] static std::optional<SsaTableDisplayValues>
        format(const domain::SsaPageResult& page,
               const std::vector<SsaDisplayColumn>& displayColumns,
               const std::shared_ptr<std::atomic_bool>& cancelToken);
    };

} // namespace ssa::presentation
