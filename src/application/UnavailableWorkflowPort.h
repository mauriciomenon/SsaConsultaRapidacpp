#pragma once

#include "ports/IWorkflowPorts.h"

namespace ssa::application {

    class UnavailableWorkflowPort final : public ports::IImportWorkflowPort,
                                          public ports::IExportPort,
                                          public ports::IDatabaseMaintenancePort,
                                          public ports::IDerivadasPort {
      public:
        [[nodiscard]] ports::WorkflowResult
        importExternalFiles(const ports::ImportExternalFilesRequest& request,
                            std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult rescan(const ports::RescanRequest& request,
                                                   std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult
        exportFilteredList(const ports::ExportFilteredListRequest& request,
                           std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult resetDatabase(std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult cleanData(std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult vacuumAnalyze(std::stop_token stopToken = {}) override;
        [[nodiscard]] ports::WorkflowResult
        cleanOrphanDerivations(std::stop_token stopToken = {}) override;

      private:
        [[nodiscard]] static ports::WorkflowResult unavailable(const char* operation);
    };

} // namespace ssa::application
