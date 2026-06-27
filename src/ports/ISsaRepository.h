#pragma once

#include "domain/SsaTypes.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ssa::ports {

    using SsaRecordConsumer = std::function<std::optional<std::string>(const domain::SsaRecord&)>;

    struct SsaReadResult {
        std::size_t rowCount{0};
        std::string error;

        [[nodiscard]] bool ok() const {
            return error.empty();
        }
    };

    class ISsaRepository {
      public:
        virtual ~ISsaRepository() = default;

        [[nodiscard]] virtual domain::SsaPageResult
        page(const domain::SsaPageRequest& request) const = 0;

        [[nodiscard]] virtual std::size_t count(const domain::SsaPageRequest& request) const = 0;

        [[nodiscard]] virtual std::optional<domain::SsaRecord>
        recordBySsaNumber(const domain::SsaNumber& number) const = 0;

        [[nodiscard]] virtual std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request) const = 0;

        [[nodiscard]] virtual SsaReadResult readAll(const domain::SsaPageRequest& request,
                                                    SsaRecordConsumer consume) const = 0;
    };

} // namespace ssa::ports
