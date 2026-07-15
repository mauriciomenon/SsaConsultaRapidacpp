#pragma once

#include "domain/SsaTypes.h"
#include "ports/ISsaRepository.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <stop_token>
#include <string_view>
#include <vector>

namespace ssa::ports {

    class ISsaBrowsePort {
      public:
        virtual ~ISsaBrowsePort() = default;

        [[nodiscard]] virtual domain::SsaPageResult page(const domain::SsaPageRequest& request,
                                                         std::stop_token stopToken = {}) const = 0;
        [[nodiscard]] virtual std::size_t count(const domain::SsaPageRequest& request,
                                                std::stop_token stopToken = {}) const = 0;
        [[nodiscard]] virtual std::optional<domain::SsaRecord>
        details(const domain::SsaNumber& number, std::stop_token stopToken = {}) const = 0;
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
