#include "DesktopSmokeCapture.h"

#include "DesktopSmokeCaptureFileSystem.h"
#include "DesktopSmokeScreenshotCapture.h"
#include "DesktopSmokeWindowLocator.h"
#include "DesktopSmokeWindowWaiter.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <memory>

namespace ssa::app::desktop {

    namespace {

        constexpr int smokeCaptureFailureExitCode = 2;
        constexpr int defaultScreenshotDelayMs = 900;

        struct DesktopSmokeCaptureOptions {
            QString screenshotPath;
            bool openPreferences = false;
            bool openAdvancedFilters = false;
            bool openAdvancedPopup = false;
            bool openDetailsWindow = false;
            bool probeLayout = false;
            int screenshotDelayMs = defaultScreenshotDelayMs;
            int windowWidth = 0;
            int windowHeight = 0;
        };

        DesktopSmokeCaptureTarget targetForRequest(const bool openPreferences,
                                                   const bool openAdvancedFilters,
                                                   const bool openDetailsWindow) {
            if (openPreferences) {
                return DesktopSmokeCaptureTarget::PreferencesWindow;
            }
            if (openDetailsWindow) {
                return DesktopSmokeCaptureTarget::DetailsWindow;
            }
            if (openAdvancedFilters) {
                return DesktopSmokeCaptureTarget::RootWindowWithAdvancedFilters;
            }
            return DesktopSmokeCaptureTarget::RootWindow;
        }

        bool parseCaptureOptions(const QCommandLineParser& parser,
                                 DesktopSmokeCaptureOptions& options) {
            bool delayParsed = true;
            bool widthParsed = true;
            bool heightParsed = true;
            options.screenshotPath = parser.value("screenshot");
            options.openPreferences = parser.isSet("open-preferences");
            options.openAdvancedFilters = parser.isSet("open-advanced-filters");
            options.openAdvancedPopup = parser.isSet("smoke-advanced-popup");
            options.openDetailsWindow = parser.isSet("open-details-window");
            options.probeLayout = parser.isSet("smoke-layout");
            const int requestedWindows = static_cast<int>(options.openPreferences) +
                                         static_cast<int>(options.openAdvancedFilters) +
                                         static_cast<int>(options.openAdvancedPopup) +
                                         static_cast<int>(options.openDetailsWindow) +
                                         static_cast<int>(options.probeLayout);
            options.screenshotDelayMs =
                parser.isSet("screenshot-delay-ms")
                    ? parser.value("screenshot-delay-ms").toInt(&delayParsed)
                    : defaultScreenshotDelayMs;
            options.windowWidth = parser.isSet("smoke-window-width")
                                      ? parser.value("smoke-window-width").toInt(&widthParsed)
                                      : 0;
            options.windowHeight = parser.isSet("smoke-window-height")
                                       ? parser.value("smoke-window-height").toInt(&heightParsed)
                                       : 0;
            return delayParsed && widthParsed && heightParsed && options.screenshotDelayMs >= 0 &&
                   options.windowWidth >= 0 && options.windowHeight >= 0 && requestedWindows <= 1;
        }

        void prepareCapture(QQmlApplicationEngine& engine,
                            const DesktopSmokeCaptureOptions& options,
                            const QPointer<DesktopSmokeController>& controller,
                            const DesktopSmokeCaptureCompletion& completion) {
            if (!DesktopSmokeCaptureFileSystem::ensureScreenshotDirectory(options.screenshotPath)) {
                completion(smokeCaptureFailureExitCode);
                return;
            }
            if ((options.openPreferences || options.openAdvancedFilters ||
                 options.openAdvancedPopup || options.openDetailsWindow) &&
                controller.isNull()) {
                completion(smokeCaptureFailureExitCode);
                return;
            }
            auto* rootWindow = engine.rootObjects().isEmpty()
                                   ? nullptr
                                   : qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
            if ((options.windowWidth > 0 || options.windowHeight > 0) && rootWindow == nullptr) {
                completion(smokeCaptureFailureExitCode);
                return;
            }
            if (options.windowWidth > 0) {
                rootWindow->setWidth(options.windowWidth);
            }
            if (options.windowHeight > 0) {
                rootWindow->setHeight(options.windowHeight);
            }
            if (options.probeLayout) {
                if ((options.windowWidth > 0 && rootWindow->width() != options.windowWidth) ||
                    (options.windowHeight > 0 && rootWindow->height() != options.windowHeight)) {
                    completion(smokeCaptureFailureExitCode);
                    return;
                }
                const QPointer<QQmlApplicationEngine> guardedEngine{&engine};
                const auto metricsReported = std::make_shared<bool>(false);
                QObject::connect(
                    controller, &DesktopSmokeController::layoutMetricsReady, &engine,
                    [guardedEngine, screenshotPath = options.screenshotPath, completion,
                     metricsReported](const QVariantMap& metrics) {
                        *metricsReported = true;
                        const auto json =
                            QJsonDocument::fromVariant(metrics).toJson(QJsonDocument::Compact);
                        qInfo().noquote() << "QML_LAYOUT_SMOKE" << json;
                        if (!metrics.value(QStringLiteral("success")).toBool() ||
                            guardedEngine.isNull()) {
                            completion(smokeCaptureFailureExitCode);
                            return;
                        }
                        DesktopSmokeWindowWaiter::capture(*guardedEngine, screenshotPath,
                                                          DesktopSmokeCaptureTarget::RootWindow,
                                                          completion);
                    },
                    Qt::SingleShotConnection);
                QTimer::singleShot(5000, &engine, [completion, metricsReported] {
                    if (!*metricsReported) {
                        *metricsReported = true;
                        completion(smokeCaptureFailureExitCode);
                    }
                });
                controller->requestLayoutProbe();
                return;
            }
            if (options.openAdvancedPopup) {
                const QPointer<QQmlApplicationEngine> guardedEngine{&engine};
                const auto metricsReported = std::make_shared<bool>(false);
                QObject::connect(
                    controller, &DesktopSmokeController::advancedPopupMetricsReady, &engine,
                    [guardedEngine, screenshotPath = options.screenshotPath, completion,
                     metricsReported](const QVariantMap& metrics) {
                        *metricsReported = true;
                        const auto json =
                            QJsonDocument::fromVariant(metrics).toJson(QJsonDocument::Compact);
                        qInfo().noquote() << "QML_POPUP_SMOKE" << json;
                        if (!metrics.value(QStringLiteral("success")).toBool() ||
                            guardedEngine.isNull()) {
                            completion(smokeCaptureFailureExitCode);
                            return;
                        }
                        DesktopSmokeWindowWaiter::capture(
                            *guardedEngine, screenshotPath,
                            DesktopSmokeCaptureTarget::RootWindowWithAdvancedFilters, completion);
                    },
                    Qt::SingleShotConnection);
                QTimer::singleShot(5000, &engine, [completion, metricsReported] {
                    if (!*metricsReported) {
                        *metricsReported = true;
                        completion(smokeCaptureFailureExitCode);
                    }
                });
                controller->requestOpenAdvancedPopup();
                return;
            }
            if (options.openDetailsWindow) {
                const QPointer<QQmlApplicationEngine> guardedEngine{&engine};
                QObject::connect(
                    controller, &DesktopSmokeController::detailsReady, &engine,
                    [guardedEngine, screenshotPath = options.screenshotPath, completion] {
                        if (guardedEngine.isNull()) {
                            completion(smokeCaptureFailureExitCode);
                            return;
                        }
                        DesktopSmokeWindowWaiter::capture(*guardedEngine, screenshotPath,
                                                          DesktopSmokeCaptureTarget::DetailsWindow,
                                                          completion);
                    },
                    Qt::SingleShotConnection);
                controller->requestOpenDetailsWindow();
                return;
            }
            if (options.openPreferences) {
                controller->requestOpenPreferences();
            }
            if (options.openAdvancedFilters) {
                controller->requestOpenAdvancedFilters();
            }
            const QPointer<QQmlApplicationEngine> guardedEngine{&engine};
            if (guardedEngine.isNull()) {
                completion(smokeCaptureFailureExitCode);
                return;
            }
            const DesktopSmokeCaptureTarget target = targetForRequest(
                options.openPreferences, options.openAdvancedFilters, options.openDetailsWindow);
            QTimer::singleShot(
                options.screenshotDelayMs, guardedEngine,
                [guardedEngine, screenshotPath = options.screenshotPath, target, completion] {
                    DesktopSmokeWindowWaiter::capture(*guardedEngine, screenshotPath, target,
                                                      completion);
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
                prepareCapture(*guardedEngine, options, guardedController, completion);
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

    void DesktopSmokeController::requestOpenAdvancedPopup() {
        emit openAdvancedPopupRequested();
    }

    void DesktopSmokeController::requestOpenDetailsWindow() {
        emit openDetailsWindowRequested();
    }

    void DesktopSmokeController::requestLayoutProbe() {
        emit layoutProbeRequested();
    }

    void DesktopSmokeController::reportAdvancedPopupMetrics(const QVariantMap& metrics) {
        emit advancedPopupMetricsReady(metrics);
    }

    void DesktopSmokeController::reportLayoutMetrics(const QVariantMap& metrics) {
        emit layoutMetricsReady(metrics);
    }

    void DesktopSmokeController::reportCaptureFailure() {
        emit captureFailureReported();
    }

    void DesktopSmokeController::reportDetailsReady() {
        emit detailsReady();
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
        const auto completed = std::make_shared<std::atomic_bool>(false);
        const DesktopSmokeCaptureCompletion completion = [completed](const int status) {
            if (completed->exchange(true)) {
                return;
            }
            if (status == 0) {
                QCoreApplication::quit();
            } else {
                QCoreApplication::exit(status);
            }
        };
        QObject::connect(
            &controller, &DesktopSmokeController::captureFailureReported, &engine,
            [completion] { completion(smokeCaptureFailureExitCode); }, Qt::SingleShotConnection);
        scheduleCapture(engine, controller, options, completion);
    }

} // namespace ssa::app::desktop
