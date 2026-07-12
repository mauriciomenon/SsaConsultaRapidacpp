#pragma once

#include "ports/IWorkflowPorts.h"

#include <memory>
#include <stop_token>

namespace ssa::ports {
    class ISsaRepository;
}

namespace ssa::infra::exporting {

    class CsvExportPort final : public ports::IExportPort {
      public:
        explicit CsvExportPort(std::shared_ptr<ports::ISsaRepository> repository);

        [[nodiscard]] ports::WorkflowResult
        exportFilteredList(const ports::ExportFilteredListRequest& request,
                           std::stop_token stopToken = {}) override;

      private:
        std::shared_ptr<ports::ISsaRepository> repository_;
    };

} // namespace ssa::infra::exporting
