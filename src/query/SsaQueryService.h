#pragma once

#include "ports/ISsaRepository.h"

#include <memory>

namespace ssa::query {

    class SsaQueryService final {
      public:
        explicit SsaQueryService(std::shared_ptr<ports::ISsaRepository> repository);

        [[nodiscard]] domain::SsaPageResult search(const domain::SsaPageRequest& request) const;
        [[nodiscard]] std::optional<domain::SsaRecord>
        details(const domain::SsaNumber& number) const;
        [[nodiscard]] std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request) const;

      private:
        std::shared_ptr<ports::ISsaRepository> repository_;
    };

} // namespace ssa::query
