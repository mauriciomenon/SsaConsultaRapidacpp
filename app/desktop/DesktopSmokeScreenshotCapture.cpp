#include "DesktopSmokeScreenshotCapture.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QImage>
#include <QPointer>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QSharedPointer>
#include <QTimer>
#include <QtConcurrent>

#include <memory>
#include <utility>

namespace ssa::app::desktop {

    namespace {

        void saveScreenshot(QImage image, const QString& screenshotPath,
                            DesktopSmokeCaptureCompletion completion) {
            auto* watcher = new QFutureWatcher<bool>{QCoreApplication::instance()};
            QObject::connect(watcher, &QFutureWatcher<bool>::finished, watcher,
                             [watcher, completion = std::move(completion)] {
                                 const bool saved = watcher->result();
                                 watcher->deleteLater();
                                 completion(saved ? 0 : 2);
                             });
            watcher->setFuture(
                QtConcurrent::run([image = std::move(image), screenshotPath]() mutable {
                    return !image.isNull() && image.save(screenshotPath);
                }));
        }

        void captureRenderedContent(const QPointer<QQuickWindow>& guardedWindow,
                                    const QString& outputPath,
                                    const DesktopSmokeCaptureCompletion& completion) {
            if (guardedWindow.isNull()) {
                completion(2);
                return;
            }
            QQuickItem* contentItem = guardedWindow->contentItem();
            if (contentItem == nullptr) {
                completion(2);
                return;
            }
            const QSharedPointer<QQuickItemGrabResult> result = contentItem->grabToImage();
            if (result.isNull()) {
                completion(2);
                return;
            }
            QObject::connect(result.data(), &QQuickItemGrabResult::ready,
                             QCoreApplication::instance(), [result, outputPath, completion] {
                                 saveScreenshot(result->image(), outputPath, completion);
                             });
        }

        DesktopSmokeCaptureCompletion completeOnce(DesktopSmokeCaptureCompletion completion,
                                                   const std::shared_ptr<bool>& done) {
            return [completion = std::move(completion), done](const int status) {
                if (*done) {
                    return;
                }
                *done = true;
                completion(status);
            };
        }

    } // namespace

    void DesktopSmokeScreenshotCapture::capture(QQuickWindow& window, const QString& outputPath,
                                                const int screenshotDelayMs,
                                                DesktopSmokeCaptureCompletion completion) {
        if (!window.isVisible()) {
            completion(2);
            return;
        }
        const QPointer<QQuickWindow> guardedWindow{&window};
        const auto done = std::make_shared<bool>(false);
        const auto captureStarted = std::make_shared<bool>(false);
        const auto claimCapture = [captureStarted] {
            if (*captureStarted) {
                return false;
            }
            *captureStarted = true;
            return true;
        };
        auto completionOnce = completeOnce(std::move(completion), done);
        QObject::connect(
            &window, &QQuickWindow::frameSwapped, &window,
            [guardedWindow, outputPath, screenshotDelayMs, completion = completionOnce,
             claimCapture] {
                if (!claimCapture()) {
                    return;
                }
                if (guardedWindow.isNull() || !guardedWindow->isVisible()) {
                    completion(2);
                    return;
                }
                if (screenshotDelayMs > 0) {
                    QTimer::singleShot(
                        screenshotDelayMs, guardedWindow, [guardedWindow, outputPath, completion] {
                            if (guardedWindow.isNull()) {
                                completion(2);
                                return;
                            }
                            captureRenderedContent(guardedWindow, outputPath, completion);
                        });
                } else {
                    captureRenderedContent(guardedWindow, outputPath, completion);
                }
            },
            Qt::SingleShotConnection);
        window.requestUpdate();
        QTimer::singleShot(1500, &window,
                           [guardedWindow, outputPath, completion = completionOnce, claimCapture] {
                               if (!claimCapture()) {
                                   return;
                               }
                               captureRenderedContent(guardedWindow, outputPath, completion);
                           });
    }

} // namespace ssa::app::desktop
