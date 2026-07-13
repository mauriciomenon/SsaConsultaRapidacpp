#pragma once

#include "DesktopSmokeCapture.h"
#include "platform/AppPaths.h"
#include "platform/StartupOptions.h"
#include "platform/SystemThemeResolver.h"
#include "presentation/MainViewModel.h"

#include <memory>

class QCommandLineParser;
class QQmlApplicationEngine;

namespace ssa::app::desktop {

    class DesktopApplicationRuntime final {
      public:
        explicit DesktopApplicationRuntime(const QCommandLineParser& parser);

        void loadMainWindow(QQmlApplicationEngine& engine);
        void installSmokeCapture(const QCommandLineParser& parser, QQmlApplicationEngine& engine);

      private:
        static void forceShutdown();

        ssa::platform::StartupOptions options_;
        ssa::platform::AppPaths paths_;
        ssa::platform::SystemThemeResolver systemThemeResolver_;
        std::unique_ptr<ssa::presentation::MainViewModel> mainViewModel_;
        DesktopSmokeController smokeController_;
    };

} // namespace ssa::app::desktop
