#pragma once

#include "platform/StartupOptions.h"
#include "ports/IDatabaseSwitchPorts.h"

#include <QStringList>

namespace ssa::platform {

    class DesktopApplicationLauncher final : public ports::IApplicationLauncher {
      public:
        explicit DesktopApplicationLauncher(const StartupOptions& options);

        [[nodiscard]] ports::ApplicationLaunchResult
        launchWithDatabase(const std::filesystem::path& path) override;
        [[nodiscard]] QStringList argumentsForDatabase(const std::filesystem::path& path) const;

      private:
        QStringList persistentArguments_;
    };

} // namespace ssa::platform
