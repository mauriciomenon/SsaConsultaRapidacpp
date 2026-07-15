#pragma once

#include "ports/IExternalProcessRunner.h"

namespace ssa::platform {

    class SupervisedProcessRunner final : public ports::IExternalProcessRunner {
      public:
        [[nodiscard]] ports::ExternalProcessResult
        run(const ports::ExternalProcessRequest& request,
            const std::stop_token& stopToken = {}) const override;
    };

} // namespace ssa::platform
