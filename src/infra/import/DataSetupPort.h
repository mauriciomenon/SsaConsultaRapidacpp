#pragma once

#include "ports/IDatabaseSwitchPorts.h"

namespace ssa::infra::importing {

    class DataSetupPort final : public ports::IDataSetupPort {
      public:
        explicit DataSetupPort(ports::ExclusiveFilePublisher& filePublisher);

        [[nodiscard]] ports::DataSetupResult execute(const ports::DataSetupRequest& request,
                                                     std::stop_token stopToken = {}) override;

      private:
        ports::ExclusiveFilePublisher& filePublisher_;
    };

} // namespace ssa::infra::importing
