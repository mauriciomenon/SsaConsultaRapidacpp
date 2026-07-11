#pragma once

#include "ports/IWorkflowPorts.h"

namespace ssa::application {

    class UnavailableWorkflowPort final : public ports::IImportWorkflowPort,
                                          public ports::IExportPort,
                                          public ports::IDatabaseMaintenancePort,
                                          public ports::IDerivadasPort {
      public:
        [[nodiscard]] ports::WorkflowResult
        importExternalFiles(const ports::ImportExternalFilesRequest& request) override;
        [[nodiscard]] ports::WorkflowResult rescan(const ports::RescanRequest& request) override;
        [[nodiscard]] ports::WorkflowResult
        exportFilteredList(const ports::ExportFilteredListRequest& request,
                           std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult resetDatabase() override;
        [[nodiscard]] ports::WorkflowResult cleanData() override;
        [[nodiscard]] ports::WorkflowResult vacuumAnalyze() override;
        [[nodiscard]] ports::WorkflowResult syncDerivadas() override;

      private:
        [[nodiscard]] static ports::WorkflowResult unavailable(const char* operation);
    };

} // namespace ssa::application
