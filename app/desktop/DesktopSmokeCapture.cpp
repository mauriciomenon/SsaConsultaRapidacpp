#include "DesktopSmokeCapture.h"

#include "DesktopSmokeCaptureFileSystem.h"
#include "DesktopSmokeWindowWaiter.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QUrl>

namespace ssa::app::desktop {

    namespace {

        constexpr int smokeCaptureFailureExitCode = 2;
        constexpr int defaultScreenshotDelayMs = 900;

        struct DesktopSmokeCaptureOptions {
            QString screenshotPath;
            bool openPreferences = false;
            bool openAdvancedFilters = false;
            int screenshotDelayMs = defaultScreenshotDelayMs;
        };

        DesktopSmokeCaptureTarget targetForRequest(const bool openPreferences,
                                                   const bool openAdvancedFilters) {
            if (openPreferences) {
                return DesktopSmokeCaptureTarget::PreferencesWindow;
            }
            if (openAdvancedFilters) {
                return DesktopSmokeCaptureTarget::RootWindowWithAdvancedFilters;
            }
            return DesktopSmokeCaptureTarget::RootWindow;
        }

        bool parseCaptureOptions(const QCommandLineParser& parser,
                                 DesktopSmokeCaptureOptions& options) {
            bool delayParsed = true;
            options.screenshotPath = parser.value("screenshot");
            options.openPreferences = parser.isSet("open-preferences");
            options.openAdvancedFilters = parser.isSet("open-advanced-filters");
            options.screenshotDelayMs =
                parser.isSet("screenshot-delay-ms")
                    ? parser.value("screenshot-delay-ms").toInt(&delayParsed)
                    : defaultScreenshotDelayMs;
            return delayParsed && options.screenshotDelayMs >= 0;
        }

        void prepareCapture(QQmlApplicationEngine& engine, const QString& screenshotPath,
                            const bool openPreferences, const bool openAdvancedFilters,
                            const int screenshotDelayMs,
                            const QPointer<DesktopSmokeController>& controller,
                            const DesktopSmokeCaptureCompletion& completion) {
            if (!DesktopSmokeCaptureFileSystem::ensureScreenshotDirectory(screenshotPath)) {
                completion(smokeCaptureFailureExitCode);
                return;
            }
            if ((openPreferences || openAdvancedFilters) && controller.isNull()) {
                completion(smokeCaptureFailureExitCode);
                return;
            }
            if (openPreferences) {
                controller->requestOpenPreferences();
            }
            if (openAdvancedFilters) {
                controller->requestOpenAdvancedFilters();
            }
            const QPointer<QQmlApplicationEngine> guardedEngine{&engine};
            if (guardedEngine.isNull()) {
                completion(smokeCaptureFailureExitCode);
                return;
            }
            const DesktopSmokeCaptureTarget target =
                targetForRequest(openPreferences, openAdvancedFilters);
            QTimer::singleShot(screenshotDelayMs, guardedEngine,
                               [guardedEngine, screenshotPath, target, completion] {
                                   DesktopSmokeWindowWaiter::capture(*guardedEngine, screenshotPath,
                                                                     target, completion);
                               });
        }

        void scheduleCapture(QQmlApplicationEngine& engine, DesktopSmokeController& controller,
                             const DesktopSmokeCaptureOptions& options,
                             const DesktopSmokeCaptureCompletion& completion) {
            const QPointer<QQmlApplicationEngine> guardedEngine{&engine};
            const QPointer<DesktopSmokeController> guardedController{&controller};
            const auto startCapture = [guardedEngine, guardedController, options, completion] {
                if (guardedEngine.isNull()) {
                    completion(smokeCaptureFailureExitCode);
                    return;
                }
                prepareCapture(*guardedEngine, options.screenshotPath, options.openPreferences,
                               options.openAdvancedFilters, options.screenshotDelayMs,
                               guardedController, completion);
            };
            if (!engine.rootObjects().isEmpty()) {
                QTimer::singleShot(0, &engine, startCapture);
                return;
            }
            QObject::connect(
                &engine, &QQmlApplicationEngine::objectCreated, &engine,
                [startCapture, completion](QObject* object, const QUrl&) {
                    if (object == nullptr) {
                        completion(smokeCaptureFailureExitCode);
                        return;
                    }
                    startCapture();
                },
                Qt::SingleShotConnection);
        }

    } // namespace

    DesktopSmokeController::DesktopSmokeController(QObject* parent) : QObject(parent) {}

    void DesktopSmokeController::requestOpenPreferences() {
        emit openPreferencesRequested();
    }

    void DesktopSmokeController::requestOpenAdvancedFilters() {
        emit openAdvancedFiltersRequested();
    }

    void DesktopSmokeCapture::installIfRequested(const QCommandLineParser& parser,
                                                 QQmlApplicationEngine& engine,
                                                 DesktopSmokeController& controller) {
        if (!parser.isSet("screenshot")) {
            return;
        }
        DesktopSmokeCaptureOptions options;
        if (!parseCaptureOptions(parser, options)) {
            QTimer::singleShot(0, &engine,
                               [] { QCoreApplication::exit(smokeCaptureFailureExitCode); });
            return;
        }
        const DesktopSmokeCaptureCompletion completion = [](const int status) {
            if (status == 0) {
                QCoreApplication::quit();
            } else {
                QCoreApplication::exit(status);
            }
        };
        scheduleCapture(engine, controller, options, completion);
    }

} // namespace ssa::app::desktop
