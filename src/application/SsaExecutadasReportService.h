#pragma once

#include "domain/SsaTypes.h"
#include "query/SsaQueryService.h"

#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace ssa::application {

    // One grouped row of the "SSA Executadas" macro report.
    struct ExecutadasReportRow {
        std::string group{};
        std::string week{};
        std::string person{};
        int count{0};
    };

    struct ExecutadasReportResult {
        std::vector<ExecutadasReportRow> rows;
        bool ok{true};
        std::string error;
    };

    // Builds the "SSA Executadas" report by executing the supplied (already
    // month-restricted) request against the query service and grouping the
    // streamed records by setor (or setor division), week and person.
    //
    // The caller is responsible for narrowing the request to the desired month
    // via advancedFilters.executionWeekStart/End so the database - not this
    // service - performs the heavy filtering.
    class SsaExecutadasReportService final {
      public:
        explicit SsaExecutadasReportService(std::shared_ptr<query::SsaQueryService> queryService);

        [[nodiscard]] ExecutadasReportResult
        buildExecutadasReport(const domain::SsaPageRequest& request, bool byDivision,
                              std::stop_token stopToken = {}) const;

      private:
        std::shared_ptr<query::SsaQueryService> queryService_;
    };

} // namespace ssa::application
