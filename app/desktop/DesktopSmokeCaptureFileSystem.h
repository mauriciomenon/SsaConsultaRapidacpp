#pragma once

#include <QString>

namespace ssa::app::desktop {

    class DesktopSmokeCaptureFileSystem final {
      public:
        [[nodiscard]] static QString absoluteScreenshotPath(const QString& screenshotPath);
        [[nodiscard]] static bool ensureScreenshotDirectory(const QString& screenshotPath);
    };

} // namespace ssa::app::desktop
