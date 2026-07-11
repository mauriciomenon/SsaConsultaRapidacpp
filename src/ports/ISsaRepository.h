#pragma once

#include "domain/SsaTypes.h"

#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
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

        [[nodiscard]] virtual domain::SsaPageResult page(const domain::SsaPageRequest& request,
                                                         std::stop_token stopToken = {}) const = 0;

        [[nodiscard]] virtual std::size_t count(const domain::SsaPageRequest& request,
                                                std::stop_token stopToken = {}) const = 0;

        [[nodiscard]] virtual std::optional<domain::SsaRecord>
        recordBySsaNumber(const domain::SsaNumber& number,
                          std::stop_token stopToken = {}) const = 0;

        // Returns the direct derived SSAs (one level): records whose
        // derivada_de matches the given SSA number.
        [[nodiscard]] virtual std::vector<domain::SsaDerivadaEntry>
        derivadasDiretas(const domain::SsaNumber& number, std::stop_token stopToken = {}) const = 0;

        [[nodiscard]] virtual std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request,
                       std::stop_token stopToken = {}) const = 0;

        [[nodiscard]] virtual std::size_t maxValueLength(std::string_view columnKey,
                                                         std::stop_token stopToken = {}) const = 0;

        [[nodiscard]] virtual SsaReadResult readAll(const domain::SsaPageRequest& request,
                                                    SsaRecordConsumer consume,
                                                    std::stop_token stopToken = {}) const = 0;
    };

} // namespace ssa::ports
