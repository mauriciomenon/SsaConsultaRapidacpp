#pragma once

#include "domain/SsaTypes.h"
#include "presentation/FilterPanelState.h"
#include "query/SearchParser.h"

#include <optional>
#include <string>

namespace ssa::presentation {

    class FilterPanelDistinctValueRequestBuilder final {
      public:
        [[nodiscard]] std::optional<domain::DistinctValuesRequest>
        columnValuesRequest(const filterpanel::FilterPanelState& state) const;

        [[nodiscard]] std::optional<domain::DistinctValuesRequest>
        columnValuesRequestFor(const filterpanel::FilterPanelState& state,
                               const std::string& columnKey) const;

        [[nodiscard]] domain::DistinctValuesRequest
        quickSectorRequest(const filterpanel::FilterPanelState& state) const;

      private:
        void applyColumnTermsExcept(domain::SsaFilterExpression& filter,
                                    const std::map<std::string, std::string>& columnFilters,
                                    const std::string& excludedColumnKey) const;

        query::SearchParser parser_;
    };

} // namespace ssa::presentation
