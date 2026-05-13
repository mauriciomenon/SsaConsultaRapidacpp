#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "query/SsaQueryService.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ssa::application {

    class SsaBrowseService final {
      public:
        explicit SsaBrowseService(std::shared_ptr<query::SsaQueryService> queryService);

        [[nodiscard]] domain::SsaPageResult page(domain::SsaPageRequest request) const;
        [[nodiscard]] std::optional<domain::SsaRecord> details(const std::string& numeroSsa) const;
        [[nodiscard]] std::vector<std::string> defaultVisibleColumns() const;
        [[nodiscard]] std::vector<std::string>
        columnsOrDefault(std::vector<std::string> requestedColumns) const;

      private:
        [[nodiscard]] domain::SsaPageRequest normalizeRequest(domain::SsaPageRequest request) const;

        std::shared_ptr<query::SsaQueryService> queryService_;
    };

} // namespace ssa::application
