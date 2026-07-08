#pragma once

#include "domain/SsaTypes.h"
#include "presentation/FilterPanelState.h"

#include <map>
#include <optional>
#include <string>

namespace ssa::presentation {

    class FilterPanelDistinctValueRequestBuilder final {
      public:
        [[nodiscard]] static std::optional<domain::DistinctValuesRequest>
        columnValuesRequest(const filterpanel::FilterPanelState& state);

        [[nodiscard]] static std::optional<domain::DistinctValuesRequest>
        columnValuesRequestFor(const filterpanel::FilterPanelState& state,
                               const std::string& columnKey);

        [[nodiscard]] static domain::DistinctValuesRequest
        quickSectorRequest(const filterpanel::FilterPanelState& state);
    };

} // namespace ssa::presentation
