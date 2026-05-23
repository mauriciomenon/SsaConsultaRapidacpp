#include "DesktopSmokeWindowWaiter.h"

#include "DesktopSmokeCaptureFileSystem.h"
#include "DesktopSmokeScreenshotCapture.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>

namespace ssa::app::desktop {

    namespace {

        constexpr int smokeCaptureFailureExitCode = 2;

    } // namespace

    void DesktopSmokeWindowWaiter::capture(QQmlApplicationEngine& engine,
                                           const QString& screenshotPath,
                                           const DesktopSmokeCaptureTarget target,
                                           const DesktopSmokeCaptureCompletion& completion) {
        QQuickWindow* captureWindow = DesktopSmokeWindowLocator::captureWindow(engine, target);
        if (captureWindow != nullptr) {
            DesktopSmokeScreenshotCapture::capture(
                *captureWindow,
                DesktopSmokeCaptureFileSystem::absoluteScreenshotPath(screenshotPath), 0,
                completion);
            return;
        }
        completion(smokeCaptureFailureExitCode);
    }

} // namespace ssa::app::desktop
