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

    domain::SsaPageResult SsaQueryService::search(const domain::SsaPageRequest& request) const {
        return repository_->page(request);
    }

    std::optional<domain::SsaRecord>
    SsaQueryService::details(const domain::SsaNumber& number) const {
        return repository_->recordBySsaNumber(number);
    }

    std::vector<std::string>
    SsaQueryService::distinctValues(const domain::DistinctValuesRequest& request) const {
        return repository_->distinctValues(request);
    }

} // namespace ssa::query
