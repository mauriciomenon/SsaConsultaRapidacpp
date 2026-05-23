#include "DesktopSmokeWindowLocator.h"

#include "DesktopSmokeObjectNames.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QWindow>

namespace ssa::app::desktop {

    namespace {

        QQuickWindow* asQuickWindow(QObject* object) {
            return qobject_cast<QQuickWindow*>(object);
        }

    } // namespace

    QQuickWindow* DesktopSmokeWindowLocator::rootWindow(QQmlApplicationEngine& engine) {
        for (QObject* root : engine.rootObjects()) {
            if (auto* window = asQuickWindow(root)) {
                return window;
            }
        }
        for (QWindow* window : QGuiApplication::topLevelWindows()) {
            if (auto* quickWindow = asQuickWindow(window)) {
                return quickWindow;
            }
        }
        return nullptr;
    }

    QQuickWindow* DesktopSmokeWindowLocator::findPreferencesDialog() {
        for (QWindow* window : QGuiApplication::topLevelWindows()) {
            auto* quickWindow = asQuickWindow(window);
            if (quickWindow != nullptr && quickWindow->isVisible() &&
                quickWindow->objectName() == smoke_object_names::preferencesDialog) {
                return quickWindow;
            }
        }
        return nullptr;
    }

    QQuickWindow* DesktopSmokeWindowLocator::captureWindow(QQmlApplicationEngine& engine,
                                                           const DesktopSmokeCaptureTarget target) {
        switch (target) {
        case DesktopSmokeCaptureTarget::RootWindow:
            return rootWindow(engine);
        case DesktopSmokeCaptureTarget::PreferencesWindow:
            return findPreferencesDialog();
        case DesktopSmokeCaptureTarget::RootWindowWithAdvancedFilters:
            return rootWindow(engine);
        }
        return nullptr;
    }

} // namespace ssa::app::desktop
