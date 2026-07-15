#pragma once

#include "domain/SsaTypes.h"
#include "ports/IExecutadasReportPort.h"

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

    // Builds the "SSA Executadas" report from the database report port.
    class SsaExecutadasReportService final {
      public:
        explicit SsaExecutadasReportService(
            std::shared_ptr<ports::IExecutadasReportPort> reportPort);

        [[nodiscard]] ExecutadasReportResult
        buildExecutadasReport(const domain::SsaPageRequest& request, bool byDivision,
                              std::stop_token stopToken = {}) const;

      private:
        std::shared_ptr<ports::IExecutadasReportPort> reportPort_;
    };

} // namespace ssa::application
