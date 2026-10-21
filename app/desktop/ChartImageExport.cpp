#include "ChartImageExport.h"

#include <QBuffer>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QSaveFile>
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

        // QSaveFile keeps the previous file until the whole new content is written and
        // committed, so a full disk cannot leave a truncated export reported as success.
        [[nodiscard]] bool writeTextFile(const QString& path, const QString& content) {
            if (path.trimmed().isEmpty()) {
                return false;
            }
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                return false;
            }
            const QByteArray bytes = content.toUtf8();
            return file.write(bytes) == bytes.size() && file.commit();
        }

        [[nodiscard]] bool writeImageFile(const QString& path, const QImage& image) {
            QSaveFile file(path);
            const QByteArray format = QFileInfo(path).suffix().toLatin1();
            return file.open(QIODevice::WriteOnly) &&
                   image.save(&file, format.isEmpty() ? nullptr : format.constData()) &&
                   file.commit();
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
        return writeImageFile(path, image);
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
            QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") +
            QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" ") +
            QStringLiteral("xmlns:xlink=\"http://www.w3.org/1999/xlink\" ") +
            QStringLiteral("width=\"%1\" height=\"%2\">").arg(image.width()).arg(image.height()) +
            QStringLiteral("<image width=\"%1\" height=\"%2\" xlink:href=\"data:image/png;base64,")
                .arg(image.width())
                .arg(image.height()) +
            QString::fromLatin1(pngBytes.toBase64()) + QStringLiteral("\"/></svg>");
        return writeTextFile(path, svg);
    }

} // namespace ssa::app::desktop
