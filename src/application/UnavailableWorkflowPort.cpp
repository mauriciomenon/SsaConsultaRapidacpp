#include "application/UnavailableWorkflowPort.h"

namespace ssa::application {

    ports::WorkflowResult
    UnavailableWorkflowPort::importExternalFiles(const ports::ImportExternalFilesRequest& request,
                                                 std::stop_token stopToken) {
        (void)request;
        (void)stopToken;
        return unavailable("import external files");
    }

    ports::WorkflowResult UnavailableWorkflowPort::rescan(const ports::RescanRequest& request,
                                                          std::stop_token stopToken) {
        (void)request;
        (void)stopToken;
        return unavailable("rescan");
    }

    ports::WorkflowResult
    UnavailableWorkflowPort::exportFilteredList(const ports::ExportFilteredListRequest& request,
                                                std::stop_token stopToken) {
        (void)request;
        (void)stopToken;
        return unavailable("export filtered list");
    }

    ports::WorkflowResult UnavailableWorkflowPort::resetDatabase(std::stop_token stopToken) {
        (void)stopToken;
        return unavailable("reset database");
    }

    ports::WorkflowResult UnavailableWorkflowPort::cleanData(std::stop_token stopToken) {
        (void)stopToken;
        return unavailable("clean data");
    }

    ports::WorkflowResult UnavailableWorkflowPort::vacuumAnalyze(std::stop_token stopToken) {
        (void)stopToken;
        return unavailable("vacuum analyze");
    }

    ports::WorkflowResult
    UnavailableWorkflowPort::importDerivations(const ports::ImportDerivationsRequest& request,
                                               std::stop_token stopToken) {
        (void)request;
        (void)stopToken;
        return unavailable("derivadas import");
    }

    bool UnavailableWorkflowPort::legacySpreadsheetConverterAvailable() const {
        return false;
    }

    ports::WorkflowResult
    UnavailableWorkflowPort::cleanOrphanDerivations(std::stop_token stopToken) {
        (void)stopToken;
        return unavailable("orphan derivation cleanup");
    }

    ports::WorkflowResult UnavailableWorkflowPort::unavailable(const char* operation) {
        return {ports::WorkflowStatus::NotImplemented,
                std::string(operation) + " workflow adapter is unavailable"};
    }

} // namespace ssa::application
