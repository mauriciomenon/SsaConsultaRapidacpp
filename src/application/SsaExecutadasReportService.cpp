#include "application/SsaExecutadasReportService.h"

#include <utility>

namespace ssa::application {

    SsaExecutadasReportService::SsaExecutadasReportService(
        std::shared_ptr<ports::IExecutadasReportPort> reportPort)
        : reportPort_(std::move(reportPort)) {}

    ExecutadasReportResult
    SsaExecutadasReportService::buildExecutadasReport(const domain::SsaPageRequest& request,
                                                      const bool byDivision,
                                                      const std::stop_token stopToken) const {
        if (!reportPort_) {
            return {{}, false, "executadas report port is unavailable"};
        }

        const auto rows = reportPort_->executadasReport(request, byDivision, stopToken);
        ExecutadasReportResult output;
        output.ok = true;
        output.rows.reserve(rows.size());
        for (const auto& row : rows) {
            output.rows.push_back(ExecutadasReportRow{row.group, row.week, row.person, row.count});
        }
        return output;
    }

} // namespace ssa::application
