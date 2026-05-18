#pragma once

#include <QString>

#include <functional>

class QQuickWindow;

namespace ssa::app::desktop {

    using DesktopSmokeCaptureCompletion = std::function<void(int)>;

    class DesktopSmokeScreenshotCapture final {
      public:
        static void capture(QQuickWindow& window, const QString& outputPath, int screenshotDelayMs,
                            DesktopSmokeCaptureCompletion completion);
    };

} // namespace ssa::app::desktop
