#include "application/UnavailableWorkflowPort.h"

namespace ssa::application {

    ports::WorkflowResult
    UnavailableWorkflowPort::importExternalFiles(const ports::ImportExternalFilesRequest& request) {
        (void)request;
        return unavailable("import external files");
    }

    ports::WorkflowResult UnavailableWorkflowPort::rescan(const ports::RescanRequest& request) {
        (void)request;
        return unavailable("rescan");
    }

    ports::WorkflowResult
    UnavailableWorkflowPort::exportFilteredList(const ports::ExportFilteredListRequest& request,
                                                std::stop_token stopToken) {
        (void)request;
        (void)stopToken;
        return unavailable("export filtered list");
    }

    ports::WorkflowResult UnavailableWorkflowPort::resetDatabase() {
        return unavailable("reset database");
    }

    ports::WorkflowResult UnavailableWorkflowPort::cleanData() {
        return unavailable("clean data");
    }

    ports::WorkflowResult UnavailableWorkflowPort::vacuumAnalyze() {
        return unavailable("vacuum analyze");
    }

    ports::WorkflowResult UnavailableWorkflowPort::syncDerivadas() {
        return unavailable("sync derivadas");
    }

    ports::WorkflowResult UnavailableWorkflowPort::unavailable(const char* operation) {
        return {ports::WorkflowStatus::NotImplemented,
                std::string(operation) + " workflow adapter is unavailable"};
    }

} // namespace ssa::application
