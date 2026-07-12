#include "application/UnavailableWorkflowPort.h"

namespace ssa::application {

    ports::WorkflowResult
    UnavailableWorkflowPort::importExternalFiles(const ports::ImportExternalFilesRequest& request,
                                                 std::stop_token) {
        (void)request;
        return unavailable("import external files");
    }

    ports::WorkflowResult UnavailableWorkflowPort::rescan(const ports::RescanRequest& request,
                                                          std::stop_token) {
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

    ports::WorkflowResult UnavailableWorkflowPort::resetDatabase(std::stop_token) {
        return unavailable("reset database");
    }

    ports::WorkflowResult UnavailableWorkflowPort::cleanData(std::stop_token) {
        return unavailable("clean data");
    }

    ports::WorkflowResult UnavailableWorkflowPort::vacuumAnalyze(std::stop_token) {
        return unavailable("vacuum analyze");
    }

    ports::WorkflowResult UnavailableWorkflowPort::syncDerivadas(std::stop_token) {
        return unavailable("sync derivadas");
    }

    ports::WorkflowResult UnavailableWorkflowPort::unavailable(const char* operation) {
        return {ports::WorkflowStatus::NotImplemented,
                std::string(operation) + " workflow adapter is unavailable"};
    }

} // namespace ssa::application
