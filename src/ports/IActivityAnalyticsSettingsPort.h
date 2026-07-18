#pragma once

#include <optional>
#include <stop_token>

namespace ssa::ports {

    class IActivityAnalyticsSettingsPort {
      public:
        virtual ~IActivityAnalyticsSettingsPort() = default;

        [[nodiscard]] virtual std::optional<int>
        warningWindowDays(std::stop_token stopToken = {}) const = 0;

        virtual void setWarningWindowDays(int days, std::stop_token stopToken = {}) = 0;
    };

} // namespace ssa::ports
