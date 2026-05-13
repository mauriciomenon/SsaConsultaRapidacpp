#include "DesktopSmokeCapture.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QWindow>

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

        QQuickWindow* rootWindow(QQmlApplicationEngine& engine) {
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

        int captureRootWindow(QQuickWindow& window, const QString& screenshotPath) {
            const QImage image = window.grabWindow();
            if (image.isNull() || !image.save(screenshotPath)) {
                return 2;
            }
            return 0;
        }

        int prepareCapture(QQmlApplicationEngine& engine, const QString& screenshotPath,
                           const bool openPreferences, DesktopSmokeController& controller) {
            if (!ensureScreenshotDirectory(screenshotPath)) {
                return 2;
            }
            auto* window = rootWindow(engine);
            if (window == nullptr) {
                return 2;
            }
            if (openPreferences) {
                controller.requestOpenPreferences();
            }
            const QString outputPath = absoluteScreenshotPath(screenshotPath);
            QObject::connect(
                window, &QQuickWindow::afterRendering, window,
                [window, outputPath] {
                    QMetaObject::invokeMethod(
                        window,
                        [window, outputPath] {
                            const int status = captureRootWindow(*window, outputPath);
                            if (status == 0) {
                                QCoreApplication::quit();
                            } else {
                                QCoreApplication::exit(status);
                            }
                        },
                        Qt::QueuedConnection);
                },
                Qt::SingleShotConnection);
            window->requestUpdate();
            return 0;
        }

    } // namespace

    DesktopSmokeController::DesktopSmokeController(QObject* parent) : QObject(parent) {}

    void DesktopSmokeController::requestOpenPreferences() {
        emit openPreferencesRequested();
    }

    void DesktopSmokeCapture::installIfRequested(const QCommandLineParser& parser,
                                                 QQmlApplicationEngine& engine,
                                                 DesktopSmokeController& controller) {
        if (!parser.isSet("screenshot")) {
            return;
        }
        const QString screenshotPath = parser.value("screenshot");
        const bool openPreferences = parser.isSet("open-preferences");
        QTimer::singleShot(0, &engine, [&engine, screenshotPath, openPreferences, &controller] {
            const int status = prepareCapture(engine, screenshotPath, openPreferences, controller);
            if (status != 0) {
                QCoreApplication::exit(status);
            }
        });
    }

} // namespace ssa::app::desktop
