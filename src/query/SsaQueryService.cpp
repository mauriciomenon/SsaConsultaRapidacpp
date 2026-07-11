#include "query/SsaQueryService.h"

#include <stdexcept>
#include <utility>

namespace ssa::query {

    SsaQueryService::SsaQueryService(std::shared_ptr<ports::ISsaRepository> repository)
        : repository_(std::move(repository)) {
        if (!repository_) {
            throw std::invalid_argument("repository is required");
        }
    }

    domain::SsaPageResult SsaQueryService::search(const domain::SsaPageRequest& request,
                                                  std::stop_token stopToken) const {
        return repository_->page(request, std::move(stopToken));
    }

    std::size_t SsaQueryService::count(const domain::SsaPageRequest& request,
                                       std::stop_token stopToken) const {
        return repository_->count(request, std::move(stopToken));
    }

    std::optional<domain::SsaRecord> SsaQueryService::details(const domain::SsaNumber& number,
                                                              std::stop_token stopToken) const {
        return repository_->recordBySsaNumber(number, std::move(stopToken));
    }

    std::vector<domain::SsaDerivadaEntry>
    SsaQueryService::derivadasDiretas(const domain::SsaNumber& number,
                                      std::stop_token stopToken) const {
        return repository_->derivadasDiretas(number, std::move(stopToken));
    }

    std::vector<std::string>
    SsaQueryService::distinctValues(const domain::DistinctValuesRequest& request,
                                    std::stop_token stopToken) const {
        return repository_->distinctValues(request, std::move(stopToken));
    }

    std::size_t SsaQueryService::maxValueLength(const std::string_view columnKey,
                                                std::stop_token stopToken) const {
        return repository_->maxValueLength(columnKey, std::move(stopToken));
    }

    ports::SsaReadResult SsaQueryService::readAll(const domain::SsaPageRequest& request,
                                                  ports::SsaRecordConsumer consume,
                                                  std::stop_token stopToken) const {
        return repository_->readAll(request, std::move(consume), std::move(stopToken));
    }

} // namespace ssa::query
