#pragma once

class QQmlApplicationEngine;
class QQuickWindow;

namespace ssa::app::desktop {

    class DesktopSmokeWindowLocator final {
      public:
        [[nodiscard]] static QQuickWindow* rootWindow(QQmlApplicationEngine& engine);
    };

} // namespace ssa::app::desktop
