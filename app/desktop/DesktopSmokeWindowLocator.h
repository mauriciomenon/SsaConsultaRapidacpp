#pragma once

class QQmlApplicationEngine;
class QQuickWindow;

namespace ssa::app::desktop {

    enum class DesktopSmokeCaptureTarget {
        RootWindow,
        PreferencesWindow,
        RootWindowWithAdvancedFilters,
        DetailsWindow,
    };

    class DesktopSmokeWindowLocator final {
      public:
        [[nodiscard]] static QQuickWindow* rootWindow(QQmlApplicationEngine& engine);
        [[nodiscard]] static QQuickWindow* findPreferencesDialog();
        [[nodiscard]] static QQuickWindow* findDetailsWindow();
        [[nodiscard]] static QQuickWindow* captureWindow(QQmlApplicationEngine& engine,
                                                         DesktopSmokeCaptureTarget target);
    };

} // namespace ssa::app::desktop
