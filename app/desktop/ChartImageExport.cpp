#include "ChartImageExport.h"

#include <QBuffer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QTimer>

namespace ssa::app::desktop {

    namespace {

        [[nodiscard]] QImage waitForGrabImage(const QSharedPointer<QQuickItemGrabResult>& result) {
            if (result.isNull()) {
                return {};
            }
            if (!result->image().isNull()) {
                return result->image();
            }
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            timeout.setInterval(5000);
            QObject::connect(result.data(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            timeout.start();
            loop.exec();
            return result->image();
        }

        [[nodiscard]] bool writeTextFile(const QString& path, const QString& content) {
            if (path.trimmed().isEmpty()) {
                return false;
            }
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                return false;
            }
            return file.write(content.toUtf8()) >= 0;
        }

    } // namespace

    ChartImageExport::ChartImageExport(QObject* parent) : QObject(parent) {}

    ChartImageExport* ChartImageExport::create(QQmlEngine* engine, QJSEngine* scriptEngine) {
        Q_UNUSED(engine);
        Q_UNUSED(scriptEngine);
        return new ChartImageExport();
    }

    bool ChartImageExport::grabItemToFile(QObject* itemObject, const QString& path) const {
        auto* item = qobject_cast<QQuickItem*>(itemObject);
        if (item == nullptr || path.trimmed().isEmpty()) {
            return false;
        }
        const QImage image = waitForGrabImage(item->grabToImage());
        if (image.isNull()) {
            return false;
        }
        return image.save(path);
    }

    bool ChartImageExport::grabItemToSvgFile(QObject* itemObject, const QString& path) const {
        auto* item = qobject_cast<QQuickItem*>(itemObject);
        if (item == nullptr || path.trimmed().isEmpty()) {
            return false;
        }
        const QImage image = waitForGrabImage(item->grabToImage());
        if (image.isNull()) {
            return false;
        }
        QByteArray pngBytes;
        QBuffer buffer(&pngBytes);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            return false;
        }
        const QString svg =
            QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
            + QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" ")
            + QStringLiteral("xmlns:xlink=\"http://www.w3.org/1999/xlink\" ")
            + QStringLiteral("width=\"%1\" height=\"%2\">").arg(image.width()).arg(image.height())
            + QStringLiteral("<image width=\"%1\" height=\"%2\" xlink:href=\"data:image/png;base64,")
                  .arg(image.width())
                  .arg(image.height())
            + QString::fromLatin1(pngBytes.toBase64()) + QStringLiteral("\"/></svg>");
        return writeTextFile(path, svg);
    }

} // namespace ssa::app::desktop
