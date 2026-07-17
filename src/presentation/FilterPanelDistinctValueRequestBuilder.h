#pragma once

#include "domain/SsaTypes.h"
#include "presentation/FilterPanelState.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>

namespace ssa::presentation {

    inline constexpr std::size_t kAdvancedDistinctValuesLimit = 5000;

    class FilterPanelDistinctValueRequestBuilder final {
      public:
        [[nodiscard]] static std::optional<domain::DistinctValuesRequest>
        columnValuesRequestFor(const filterpanel::FilterPanelState& state,
                               const std::string& columnKey);

        [[nodiscard]] static domain::DistinctValuesRequest
        quickSectorRequest(const filterpanel::FilterPanelState& state);
    };

} // namespace ssa::presentation
