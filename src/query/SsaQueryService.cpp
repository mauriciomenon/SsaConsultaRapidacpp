#include "query/SsaQueryService.h"

#include "ports/OperationError.h"

#include <stdexcept>
#include <utility>

namespace ssa::query {

    domain::SsaPageResult SsaQueryService::page(const domain::SsaPageRequest& request,
                                                const std::stop_token stopToken) const {
        return search(request, stopToken);
    }

    SsaQueryService::SsaQueryService(std::shared_ptr<ports::ISsaRepository> repository,
                                     std::shared_ptr<ports::IExecutadasReportPort> reportPort)
        : repository_(std::move(repository)),
          reportPort_(reportPort != nullptr
                          ? std::move(reportPort)
                          : std::dynamic_pointer_cast<ports::IExecutadasReportPort>(repository_)) {
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

    std::vector<domain::SsaExecutadasReportRow>
    SsaQueryService::executadasReport(const domain::SsaPageRequest& request, const bool byDivision,
                                      const std::stop_token stopToken) const {
        if (!reportPort_) {
            throw ports::OperationError("Relatorio de executadas indisponivel",
                                        "executadas report port is not configured");
        }
        return reportPort_->executadasReport(request, byDivision, std::move(stopToken));
    }

} // namespace ssa::query
