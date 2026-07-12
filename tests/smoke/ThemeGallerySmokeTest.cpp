#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QtQml/qqml.h>

#include <memory>

namespace {

    QDir repositoryRoot() {
        QDir root = QFileInfo(QString::fromUtf8(__FILE__)).dir();
        if (!root.cdUp() || !root.cdUp()) {
            qFatal("test repository root could not be resolved");
        }
        return root;
    }

    class ThemeGallerySmokeTest final : public QObject {
        Q_OBJECT

      private slots:
        void initTestCase() {
            const QDir root = repositoryRoot();
            const QUrl themeUrl =
                QUrl::fromLocalFile(root.filePath(QStringLiteral("app/desktop/qml/Theme.qml")));
            QVERIFY(qmlRegisterSingletonType(themeUrl, "SsaConsultaRapida", 1, 0, "Theme") >= 0);
            const QDir components(root.filePath(QStringLiteral("app/desktop/qml/components")));
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("ActionButton.qml")),
                                    "SsaConsultaRapida", 1, 0, "ActionButton") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppTextField.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppTextField") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppComboBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppComboBox") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppCheckBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppCheckBox") >= 0);
        }

        void new_themes_render_at_normal_and_narrow_widths() {
            static const QStringList themes{
                QStringLiteral("ayu-light"),      QStringLiteral("ayu-mirage"),
                QStringLiteral("flexoki-dark"),   QStringLiteral("flexoki-light"),
                QStringLiteral("kanagawa"),       QStringLiteral("kanagawa-dragon"),
                QStringLiteral("rose-pine"),      QStringLiteral("rose-pine-moon"),
                QStringLiteral("rose-pine-dawn"), QStringLiteral("primer-dark"),
                QStringLiteral("primer-light"),   QStringLiteral("oxocarbon-light"),
            };
            const QDir root = repositoryRoot();
            QQmlEngine engine;
            QQmlComponent component(&engine, QUrl::fromLocalFile(root.filePath(
                                                 QStringLiteral("tests/smoke/ThemeLab.qml"))));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            std::unique_ptr<QObject> object(component.create());
            QVERIFY2(object != nullptr, qPrintable(component.errorString()));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            auto* content = window->findChild<QQuickItem*>(QStringLiteral("galleryContent"));
            QVERIFY(content != nullptr);

            const QString outputRoot = QDir(QCoreApplication::applicationDirPath())
                                           .filePath(QStringLiteral("theme-gallery"));
            QVERIFY(QDir().mkpath(outputRoot));
            for (const int width : {960, 720}) {
                window->setWidth(width);
                window->setHeight(720);
                window->show();
                for (const QString& theme : themes) {
                    QSignalSpy frameSpy(window, &QQuickWindow::frameSwapped);
                    QVERIFY(window->setProperty("selectedTheme", theme));
                    window->requestUpdate();
                    QTRY_COMPARE_WITH_TIMEOUT(window->property("renderedTheme").toString(), theme,
                                              1000);
                    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() > 0, 1000);

                    QVERIFY(content->x() >= 0.0);
                    QVERIFY(content->y() >= 0.0);
                    QVERIFY(content->x() + content->width() <= window->width());
                    QVERIFY(content->y() + content->height() <= window->height());
                    const QImage image = window->grabWindow();
                    QVERIFY(!image.isNull());
                    QCOMPARE(image.size(), QSize(width, 720));
                    const QString directory = QDir(outputRoot).filePath(QString::number(width));
                    QVERIFY(QDir().mkpath(directory));
                    QVERIFY(image.save(QDir(directory).filePath(theme + QStringLiteral(".png"))));
                }
            }
            window->hide();
        }
    };

} // namespace

QTEST_MAIN(ThemeGallerySmokeTest)

#include "ThemeGallerySmokeTest.moc"
