#include "application/SsaBrowseService.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ssa::application {

    SsaBrowseService::SsaBrowseService(std::shared_ptr<query::SsaQueryService> queryService)
        : queryService_(std::move(queryService)) {
        if (!queryService_) {
            throw std::invalid_argument("query service is required");
        }
    }

    domain::SsaPageResult SsaBrowseService::page(const domain::SsaPageRequest& request,
                                                 const std::stop_token stopToken) const {
        return queryService_->search(normalizeRequest(request), stopToken);
    }

    std::size_t SsaBrowseService::count(const domain::SsaPageRequest& request,
                                        const std::stop_token stopToken) const {
        return queryService_->count(request, stopToken);
    }

    std::optional<domain::SsaRecord>
    SsaBrowseService::details(const domain::SsaNumber& number,
                              const std::stop_token stopToken) const {
        return queryService_->details(number, stopToken);
    }

    std::vector<domain::SsaDerivadaEntry>
    SsaBrowseService::derivadasDiretas(const domain::SsaNumber& number,
                                       const std::stop_token stopToken) const {
        return queryService_->derivadasDiretas(number, stopToken);
    }

    std::vector<std::string>
    SsaBrowseService::distinctValues(const domain::DistinctValuesRequest& request,
                                     const std::stop_token stopToken) const {
        return queryService_->distinctValues(request, stopToken);
    }

    std::size_t SsaBrowseService::maxValueLength(const std::string_view columnKey,
                                                 const std::stop_token stopToken) const {
        return queryService_->maxValueLength(columnKey, stopToken);
    }

    ports::SsaReadResult SsaBrowseService::readAll(const domain::SsaPageRequest& request,
                                                   ports::SsaRecordConsumer consume,
                                                   const std::stop_token stopToken) const {
        return queryService_->readAll(normalizeRequest(request), std::move(consume), stopToken);
    }

    std::optional<domain::SsaRecord> SsaBrowseService::details(const std::string& numeroSsa) const {
        return details(domain::SsaNumber{numeroSsa});
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
        return std::move(requestedColumns);
    }

    domain::SsaPageRequest
    SsaBrowseService::normalizeRequest(domain::SsaPageRequest request) const {
        request.visibleColumns = columnsOrDefault(std::move(request.visibleColumns));
        request.pageSize =
            std::clamp(request.pageSize, static_cast<std::size_t>(domain::kMinPageSize),
                       static_cast<std::size_t>(domain::kMaxPageSize));
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
