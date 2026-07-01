#include "DesktopSmokeWindowLocator.h"

#include "DesktopSmokeObjectNames.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QWindow>

#include <algorithm>

namespace ssa::app::desktop {

    namespace {

        QQuickWindow* asQuickWindow(QObject* object) {
            return qobject_cast<QQuickWindow*>(object);
        }

    } // namespace

    QQuickWindow* DesktopSmokeWindowLocator::rootWindow(QQmlApplicationEngine& engine) {
        const auto roots = engine.rootObjects();
        const auto rootWindow = std::ranges::find_if(
            roots, [](QObject* root) { return asQuickWindow(root) != nullptr; });
        if (rootWindow != roots.end()) {
            return asQuickWindow(*rootWindow);
        }

        const auto windows = QGuiApplication::topLevelWindows();
        const auto quickWindow = std::ranges::find_if(
            windows, [](QWindow* window) { return asQuickWindow(window) != nullptr; });
        return quickWindow == windows.end() ? nullptr : asQuickWindow(*quickWindow);
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

    QQuickWindow* DesktopSmokeWindowLocator::findDetailsWindow() {
        for (QWindow* window : QGuiApplication::topLevelWindows()) {
            auto* quickWindow = asQuickWindow(window);
            if (quickWindow != nullptr && quickWindow->isVisible() &&
                quickWindow->objectName() == smoke_object_names::detailsWindow) {
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
        case DesktopSmokeCaptureTarget::DetailsWindow:
            return findDetailsWindow();
        }
        return nullptr;
    }

} // namespace ssa::app::desktop
