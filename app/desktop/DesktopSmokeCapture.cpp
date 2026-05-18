#include "DesktopSmokeCapture.h"

#include "DesktopSmokeScreenshotCapture.h"
#include "DesktopSmokeWindowLocator.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

namespace ssa::app::desktop {

    namespace {

        QString absoluteScreenshotPath(const QString& screenshotPath) {
            return QFileInfo{screenshotPath}.absoluteFilePath();
        }

        bool ensureScreenshotDirectory(const QString& screenshotPath) {
            const QFileInfo fileInfo{absoluteScreenshotPath(screenshotPath)};
            const QDir parent = fileInfo.absoluteDir();
            return parent.exists() || QDir{}.mkpath(parent.absolutePath());
        }

        int prepareCapture(QQmlApplicationEngine& engine, const QString& screenshotPath,
                           const bool openPreferences, const bool openAdvancedFilters,
                           const int screenshotDelayMs, DesktopSmokeController& controller,
                           const DesktopSmokeCaptureCompletion& completion) {
            if (!ensureScreenshotDirectory(screenshotPath)) {
                return 2;
            }
            auto* window = DesktopSmokeWindowLocator::rootWindow(engine);
            if (window == nullptr) {
                return 2;
            }
            if (openPreferences) {
                controller.requestOpenPreferences();
            }
            if (openAdvancedFilters) {
                controller.requestOpenAdvancedFilters();
            }
            DesktopSmokeScreenshotCapture::capture(*window, absoluteScreenshotPath(screenshotPath),
                                                   screenshotDelayMs, completion);
            return 0;
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
        const QString screenshotPath = parser.value("screenshot");
        const bool openPreferences = parser.isSet("open-preferences");
        const bool openAdvancedFilters = parser.isSet("open-advanced-filters");
        bool delayParsed = true;
        const int screenshotDelayMs = parser.isSet("screenshot-delay-ms")
                                          ? parser.value("screenshot-delay-ms").toInt(&delayParsed)
                                          : 900;
        if (!delayParsed || screenshotDelayMs < 0) {
            QTimer::singleShot(0, &engine, [] { QCoreApplication::exit(2); });
            return;
        }
        const DesktopSmokeCaptureCompletion completion = [](const int status) {
            if (status == 0) {
                QCoreApplication::quit();
            } else {
                QCoreApplication::exit(status);
            }
        };
        const auto startCapture = [&engine, screenshotPath, openPreferences, openAdvancedFilters,
                                   screenshotDelayMs, &controller, completion] {
            const int status =
                prepareCapture(engine, screenshotPath, openPreferences, openAdvancedFilters,
                               screenshotDelayMs, controller, completion);
            if (status != 0) {
                completion(status);
            }
        };
        if (!engine.rootObjects().isEmpty()) {
            QTimer::singleShot(0, &engine, startCapture);
            return;
        }
        QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreated, &engine,
            [startCapture, completion](QObject* object, const QUrl&) {
                if (object == nullptr) {
                    completion(2);
                    return;
                }
                startCapture();
            },
            Qt::SingleShotConnection);
    }

} // namespace ssa::app::desktop
