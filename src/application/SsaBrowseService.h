#pragma once

#include "domain/ColumnCatalog.h"
#include "domain/SsaTypes.h"
#include "ports/ISsaBrowsePort.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ssa::application {

    class SsaBrowseService final : public ports::ISsaBrowsePort {
      public:
        explicit SsaBrowseService(std::shared_ptr<ports::ISsaBrowsePort> browsePort);

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

        [[nodiscard]] std::optional<domain::SsaRecord> details(const std::string& numeroSsa) const;
        [[nodiscard]] std::vector<std::string> defaultVisibleColumns() const;
        [[nodiscard]] std::vector<std::string>
        columnsOrDefault(std::vector<std::string> requestedColumns) const;

      private:
        [[nodiscard]] domain::SsaPageRequest normalizeRequest(domain::SsaPageRequest request) const;

        std::shared_ptr<ports::ISsaBrowsePort> browsePort_;
    };

} // namespace ssa::application
