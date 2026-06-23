#pragma once

#include "ports/IWorkflowPorts.h"

#include <memory>

namespace ssa::application {

    class SsaWorkflowService final {
      public:
        explicit SsaWorkflowService(
            std::shared_ptr<ports::IImportWorkflowPort> importPort = nullptr,
            std::shared_ptr<ports::IExportPort> exportPort = nullptr,
            std::shared_ptr<ports::IDatabaseMaintenancePort> maintenancePort = nullptr,
            std::shared_ptr<ports::IDerivadasPort> derivadasPort = nullptr);

        [[nodiscard]] ports::WorkflowResult
        importExternalFiles(const ports::ImportExternalFilesRequest& request) const;
        [[nodiscard]] ports::WorkflowResult rescan(const ports::RescanRequest& request) const;
        [[nodiscard]] ports::WorkflowResult
        exportFilteredList(const ports::ExportFilteredListRequest& request) const;
        [[nodiscard]] ports::WorkflowResult resetDatabase() const;
        [[nodiscard]] ports::WorkflowResult cleanData() const;
        [[nodiscard]] ports::WorkflowResult vacuumAnalyze() const;
        [[nodiscard]] ports::WorkflowResult syncDerivadas() const;

      private:
        [[nodiscard]] static ports::WorkflowResult notImplemented(const char* operation);

        std::shared_ptr<ports::IImportWorkflowPort> importPort_;
        std::shared_ptr<ports::IExportPort> exportPort_;
        std::shared_ptr<ports::IDatabaseMaintenancePort> maintenancePort_;
        std::shared_ptr<ports::IDerivadasPort> derivadasPort_;
    };

} // namespace ssa::application
