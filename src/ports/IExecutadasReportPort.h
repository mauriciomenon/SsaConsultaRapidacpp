#pragma once

#include "domain/SsaTypes.h"

#include <stop_token>
#include <vector>

namespace ssa::ports {

    class IExecutadasReportPort {
      public:
        virtual ~IExecutadasReportPort() = default;

        [[nodiscard]] virtual std::vector<domain::SsaExecutadasReportRow>
        executadasReport(const domain::SsaPageRequest& request, bool byDivision,
                         std::stop_token stopToken = {}) const = 0;
    };

} // namespace ssa::ports
