#pragma once

#include "ports/IExecutadasReportPort.h"
#include "ports/ISsaBrowsePort.h"

#include <memory>
#include <stop_token>

namespace ssa::query {

    class SsaQueryService final : public ports::ISsaBrowsePort,
                                  public ports::IExecutadasReportPort {
      public:
        explicit SsaQueryService(
            std::shared_ptr<ports::ISsaRepository> repository,
            std::shared_ptr<ports::IExecutadasReportPort> reportPort = nullptr);

        [[nodiscard]] domain::SsaPageResult page(const domain::SsaPageRequest& request,
                                                 std::stop_token stopToken = {}) const override;
        [[nodiscard]] std::size_t count(const domain::SsaPageRequest& request,
                                        std::stop_token stopToken = {}) const override;
        [[nodiscard]] std::optional<domain::SsaRecord>
        details(const domain::SsaNumber& number, std::stop_token stopToken = {}) const override;
        [[nodiscard]] std::vector<domain::SsaDerivadaEntry>
        derivadasDiretas(const domain::SsaNumber& number,
                         std::stop_token stopToken = {}) const override;
        [[nodiscard]] std::vector<std::string>
        distinctValues(const domain::DistinctValuesRequest& request,
                       std::stop_token stopToken = {}) const override;
        [[nodiscard]] std::size_t maxValueLength(std::string_view columnKey,
                                                 std::stop_token stopToken = {}) const override;
        [[nodiscard]] ports::SsaReadResult readAll(const domain::SsaPageRequest& request,
                                                   ports::SsaRecordConsumer consume,
                                                   std::stop_token stopToken = {}) const override;

        [[nodiscard]] std::vector<domain::SsaExecutadasReportRow>
        executadasReport(const domain::SsaPageRequest& request, bool byDivision,
                         std::stop_token stopToken = {}) const override;

        [[nodiscard]] domain::SsaPageResult search(const domain::SsaPageRequest& request,
                                                   std::stop_token stopToken = {}) const;

      private:
        std::shared_ptr<ports::ISsaRepository> repository_;
        std::shared_ptr<ports::IExecutadasReportPort> reportPort_;
    };

} // namespace ssa::query
