#include "DesktopSmokeWindowLocator.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QWindow>

namespace ssa::app::desktop {

    QQuickWindow* DesktopSmokeWindowLocator::rootWindow(QQmlApplicationEngine& engine) {
        for (QObject* root : engine.rootObjects()) {
            if (auto* window = qobject_cast<QQuickWindow*>(root)) {
                return window;
            }
        }
        for (QWindow* window : QGuiApplication::topLevelWindows()) {
            if (auto* quickWindow = qobject_cast<QQuickWindow*>(window)) {
                return quickWindow;
            }
        }
        return nullptr;
    }

} // namespace ssa::app::desktop
