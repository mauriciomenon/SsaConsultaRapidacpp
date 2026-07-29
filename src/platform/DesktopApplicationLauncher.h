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
        [[nodiscard]] ports::ApplicationLaunchResult
        launchConfigured(const ports::ApplicationLaunchTargets& targets) override;
        [[nodiscard]] QStringList argumentsForDatabase(const std::filesystem::path& path) const;
        [[nodiscard]] QStringList
        argumentsForConfigured(const ports::ApplicationLaunchTargets& targets) const;

      private:
        QStringList persistentArguments_;
        QString samBaseUrl_;
    };

} // namespace ssa::platform
