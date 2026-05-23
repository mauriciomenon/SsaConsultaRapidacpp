#pragma once

#include "DesktopSmokeScreenshotCapture.h"
#include "DesktopSmokeWindowLocator.h"

#include <QString>

class QQmlApplicationEngine;

namespace ssa::app::desktop {

    class DesktopSmokeWindowWaiter final {
      public:
        static void capture(QQmlApplicationEngine& engine, const QString& screenshotPath,
                            DesktopSmokeCaptureTarget target,
                            const DesktopSmokeCaptureCompletion& completion);
    };

} // namespace ssa::app::desktop
