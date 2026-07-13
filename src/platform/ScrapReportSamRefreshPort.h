#pragma once

#include "ports/IWorkflowPorts.h"

#include <filesystem>
#include <memory>

class QTemporaryDir;

namespace ssa::platform {

    class ScrapReportSamRefreshPort final : public ports::ISamRefreshPort {
      public:
        explicit ScrapReportSamRefreshPort(std::filesystem::path uvExecutable = {});
        ~ScrapReportSamRefreshPort() override;

        [[nodiscard]] ports::SamFetchResult fetch(const ports::SamRefreshRequest& request,
                                                  std::stop_token stopToken = {}) override;
        bool discardArtifacts() override;

      private:
        std::filesystem::path uvExecutable_;
        std::unique_ptr<QTemporaryDir> activeOutput_;
    };

} // namespace ssa::platform
