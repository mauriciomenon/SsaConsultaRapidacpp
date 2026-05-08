#pragma once

#include "domain/SsaTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace ssa::ports {

    class ISsaRepository {
      public:
        virtual ~ISsaRepository() = default;

        [[nodiscard]] virtual domain::SsaPageResult
        page(const domain::SsaPageRequest& request) const = 0;

        [[nodiscard]] virtual std::size_t count(const domain::SsaPageRequest& request) const = 0;

        [[nodiscard]] virtual std::optional<domain::SsaRecord>
        recordById(const domain::SsaId& id) const = 0;

        [[nodiscard]] virtual std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request) const = 0;
    };

} // namespace ssa::ports
