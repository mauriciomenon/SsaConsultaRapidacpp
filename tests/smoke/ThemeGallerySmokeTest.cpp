#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QtGlobal>
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

    QQuickItem* findVisualChild(QQuickItem& root, const QString& objectName) {
        if (root.objectName() == objectName) {
            return &root;
        }
        for (auto* child : root.childItems()) {
            if (auto* found = findVisualChild(*child, objectName)) {
                return found;
            }
        }
        return nullptr;
    }

    QQuickItem* findFontBearingChild(QQuickItem& root) {
        for (auto* child : root.childItems()) {
            if (child->property("font").isValid()) {
                return child;
            }
            if (auto* found = findFontBearingChild(*child)) {
                return found;
            }
        }
        return nullptr;
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
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppSpinBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppSpinBox") >= 0);
            QVERIFY(qmlRegisterType(QUrl::fromLocalFile(components.filePath("AppCheckBox.qml")),
                                    "SsaConsultaRapida", 1, 0, "AppCheckBox") >= 0);
        }

        void action_button_uses_pixels_and_other_shared_controls_use_point_sizes() {
            static constexpr auto kHarness = R"QML(
import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

ApplicationWindow {
    width: 420
    height: 340
    visible: true

    ActionButton {
        objectName: "typeScaleActionButton"
        x: 16
        y: 16
        width: 180
        text: "Agypq"
    }

    AppComboBox {
        objectName: "typeScaleComboBox"
        x: 16
        y: 76
        width: 180
        model: ["Agypq"]
        currentIndex: 0
    }

    AppSpinBox {
        objectName: "typeScaleSpinBox"
        x: 16
        y: 136
        width: 180
        from: 0
        to: 999
        value: 888
    }

    AppTextField {
        objectName: "typeScaleTextField"
        x: 216
        y: 16
        width: 180
        height: Theme.controlHeight
        text: "Agypq"
    }

    AppCheckBox {
        objectName: "typeScaleCheckBox"
        x: 216
        y: 76
        width: 180
        height: Theme.controlHeight
        text: "Agypq"
        checked: true
    }
}
)QML";

            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(kHarness, QUrl(QStringLiteral("inmemory:/TypeScaleHarness.qml")));
            QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));

            std::unique_ptr<QObject> object(component.create());
            QVERIFY2(object != nullptr, qPrintable(component.errorString()));
            auto* window = qobject_cast<QQuickWindow*>(object.get());
            QVERIFY(window != nullptr);
            window->show();

            const auto usesPointSize = [](const QObject& item) {
                const QFont font = qvariant_cast<QFont>(item.property("font"));
                return font.pointSizeF() > 0.0 && font.pixelSize() == -1;
            };
            const auto usesPixelSize = [](const QObject& item) {
                const QFont font = qvariant_cast<QFont>(item.property("font"));
                return font.pointSizeF() == -1.0 && font.pixelSize() == 12;
            };
            const auto pointSize = [](const QObject& item) {
                return qvariant_cast<QFont>(item.property("font")).pointSizeF();
            };
            const auto contentItem = [](QQuickItem& control) {
                if (auto* content = qvariant_cast<QQuickItem*>(control.property("contentItem"))) {
                    return content;
                }
                return findFontBearingChild(control);
            };
            const auto contentFits = [&contentItem](QQuickItem& control) {
                const auto* content = contentItem(control);
                if (content == nullptr) {
                    return false;
                }
                const auto availableHeight = control.property("availableHeight").toReal();
                const auto height = availableHeight > 0.0 ? availableHeight : control.height();
                return height >= content->implicitHeight();
            };
            const auto scaleFont = [](QQuickItem& control) {
                QFont font = qvariant_cast<QFont>(control.property("font"));
                font.setPointSizeF(font.pointSizeF() * 1.5);
                return control.setProperty("font", font);
            };

            auto* actionButton =
                window->findChild<QQuickItem*>(QStringLiteral("typeScaleActionButton"));
            auto* comboBox = window->findChild<QQuickItem*>(QStringLiteral("typeScaleComboBox"));
            auto* spinBox = window->findChild<QQuickItem*>(QStringLiteral("typeScaleSpinBox"));
            auto* textField = window->findChild<QQuickItem*>(QStringLiteral("typeScaleTextField"));
            auto* checkBox = window->findChild<QQuickItem*>(QStringLiteral("typeScaleCheckBox"));
            auto* comboPopup = window->findChild<QObject*>(QStringLiteral("appComboBoxPopup"));
            QVERIFY(actionButton != nullptr);
            QVERIFY(comboBox != nullptr);
            QVERIFY(spinBox != nullptr);
            QVERIFY(textField != nullptr);
            QVERIFY(checkBox != nullptr);
            QVERIFY(comboPopup != nullptr);
            QVERIFY(QMetaObject::invokeMethod(comboPopup, "open"));
            auto* popupContent = qvariant_cast<QQuickItem*>(comboPopup->property("contentItem"));
            QVERIFY(popupContent != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(
                findVisualChild(*popupContent, QStringLiteral("appComboBoxDelegate")) != nullptr,
                1000);
            auto* comboDelegate =
                findVisualChild(*popupContent, QStringLiteral("appComboBoxDelegate"));
            QVERIFY(comboDelegate != nullptr);
            const qreal comboDelegatePointSize = pointSize(*comboDelegate);

            QVERIFY(usesPixelSize(*actionButton));
            const auto* actionContent = contentItem(*actionButton);
            QVERIFY(actionContent != nullptr);
            QVERIFY(usesPixelSize(*actionContent));
            QVERIFY(contentFits(*actionButton));

            for (QQuickItem* control : {comboBox, spinBox, comboDelegate}) {
                QVERIFY(usesPointSize(*control));
                const auto* content = contentItem(*control);
                QVERIFY(content != nullptr);
                QVERIFY(usesPointSize(*content));
                QVERIFY(contentFits(*control));
            }

            for (QQuickItem* control : {textField, checkBox}) {
                QVERIFY(usesPointSize(*control));
                const auto* content = contentItem(*control);
                QVERIFY2(
                    content != nullptr,
                    qPrintable(
                        QStringLiteral("missing content item: %1").arg(control->objectName())));
                QVERIFY(usesPointSize(*content));
                QVERIFY2(
                    contentFits(*control),
                    qPrintable(QStringLiteral("content does not fit: %1 available=%2 implicit=%3")
                                   .arg(control->objectName())
                                   .arg(control->property("availableHeight").toReal())
                                   .arg(content->implicitHeight())));
            }

            for (QQuickItem* control : {comboBox, spinBox}) {
                const qreal initialPointSize = pointSize(*control);
                QVERIFY(initialPointSize > 0.0);
                QVERIFY(scaleFont(*control));
                QTRY_VERIFY_WITH_TIMEOUT(
                    qFuzzyCompare(pointSize(*control), initialPointSize * 1.5) &&
                        contentFits(*control),
                    1000);
            }
            QTRY_VERIFY_WITH_TIMEOUT(
                qFuzzyCompare(pointSize(*comboDelegate), comboDelegatePointSize * 1.5) &&
                    contentFits(*comboDelegate),
                1000);
            window->hide();
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
