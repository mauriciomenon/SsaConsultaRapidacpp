#pragma once

#include "ports/ISsaRepository.h"

#include <memory>
#include <stop_token>

namespace ssa::query {

    class SsaQueryService final {
      public:
        explicit SsaQueryService(std::shared_ptr<ports::ISsaRepository> repository);

        [[nodiscard]] domain::SsaPageResult search(const domain::SsaPageRequest& request,
                                                   std::stop_token stopToken = {}) const;
        [[nodiscard]] std::size_t count(const domain::SsaPageRequest& request,
                                        std::stop_token stopToken = {}) const;
        [[nodiscard]] std::optional<domain::SsaRecord>
        details(const domain::SsaNumber& number, std::stop_token stopToken = {}) const;
        [[nodiscard]] std::vector<domain::SsaDerivadaEntry>
        derivadasDiretas(const domain::SsaNumber& number, std::stop_token stopToken = {}) const;
        [[nodiscard]] std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request,
                       std::stop_token stopToken = {}) const;
        [[nodiscard]] std::size_t maxValueLength(std::string_view columnKey,
                                                 std::stop_token stopToken = {}) const;
        [[nodiscard]] ports::SsaReadResult readAll(const domain::SsaPageRequest& request,
                                                   ports::SsaRecordConsumer consume,
                                                   std::stop_token stopToken = {}) const;

      private:
        std::shared_ptr<ports::ISsaRepository> repository_;
    };

} // namespace ssa::query
