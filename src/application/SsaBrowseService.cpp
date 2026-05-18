#include "application/SsaBrowseService.h"

#include <stdexcept>
#include <utility>

namespace ssa::application {

    SsaBrowseService::SsaBrowseService(std::shared_ptr<query::SsaQueryService> queryService)
        : queryService_(std::move(queryService)) {
        if (!queryService_) {
            throw std::invalid_argument("query service is required");
        }
    }

    domain::SsaPageResult SsaBrowseService::page(domain::SsaPageRequest request) const {
        return queryService_->search(normalizeRequest(std::move(request)));
    }

    std::optional<domain::SsaRecord> SsaBrowseService::details(const std::string& numeroSsa) const {
        return queryService_->details(domain::SsaNumber{numeroSsa});
    }

    std::vector<std::string> SsaBrowseService::defaultVisibleColumns() const {
        // ColumnCatalog is the product column contract, not a runtime schema source.
        return domain::ColumnCatalog::defaultVisibleKeys();
    }

    std::vector<std::string>
    SsaBrowseService::columnsOrDefault(std::vector<std::string> requestedColumns) const {
        if (requestedColumns.empty()) {
            return defaultVisibleColumns();
        }
        for (const auto& key : requestedColumns) {
            if (!domain::ColumnCatalog::contains(key)) {
                throw std::invalid_argument("unknown column: " + key);
            }
        }
        return requestedColumns;
    }

    domain::SsaPageRequest
    SsaBrowseService::normalizeRequest(domain::SsaPageRequest request) const {
        request.visibleColumns = columnsOrDefault(std::move(request.visibleColumns));
        request.pageSize =
            static_cast<std::size_t>(domain::clampPageSize(static_cast<int>(request.pageSize)));
        if (!request.sort.columnKey.empty()) {
            if (!domain::ColumnCatalog::contains(request.sort.columnKey)) {
                throw std::invalid_argument("unknown sort column: " + request.sort.columnKey);
            }
            request.sort.statusLast =
                domain::shouldApplyStatusLastTieBreaker(request.sort.columnKey);
        }
        return request;
    }

} // namespace ssa::application
