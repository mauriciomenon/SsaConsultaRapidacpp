#pragma once

#include "ports/ISsaRepository.h"
#include "query/SqlQueryBuilder.h"

#include <filesystem>
#include <optional>

namespace ssa::infra::sqlite {

    class SqliteSsaRepository final : public ports::ISsaRepository {
      public:
        explicit SqliteSsaRepository(std::filesystem::path dbPath);

        [[nodiscard]] domain::SsaPageResult
        page(const domain::SsaPageRequest& request) const override;

        [[nodiscard]] std::size_t count(const domain::SsaPageRequest& request) const override;

        [[nodiscard]] std::optional<domain::SsaRecord>
        recordById(const domain::SsaId& id) const override;

        [[nodiscard]] std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request) const override;

      private:
        std::filesystem::path dbPath_;
        query::SqlQueryBuilder queryBuilder_;
    };

} // namespace ssa::infra::sqlite
