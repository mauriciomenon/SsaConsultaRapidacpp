#pragma once

#include "ports/ISsaRepository.h"
#include "ports/IWorkflowPorts.h"

#include <memory>

namespace ssa::infra::exporting {

    class CsvExportPort final : public ports::IExportPort {
      public:
        explicit CsvExportPort(std::shared_ptr<ports::ISsaRepository> repository);

        [[nodiscard]] ports::WorkflowResult
        exportFilteredList(const ports::ExportFilteredListRequest& request) override;

      private:
        std::shared_ptr<ports::ISsaRepository> repository_;
    };

} // namespace ssa::infra::exporting
