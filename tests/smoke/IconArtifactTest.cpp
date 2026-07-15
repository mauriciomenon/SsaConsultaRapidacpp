#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QTest>

class IconArtifactTest final : public QObject {
    Q_OBJECT

  private slots:
    void mac_bundle_contains_application_icon() {
#ifdef Q_OS_MACOS
        const auto iconPath = QDir(QStringLiteral(SSA_BUILD_DIR))
                                  .filePath(QStringLiteral(
                                      "ssa_consulta_rapida.app/Contents/Resources/app_icon.icns"));
        QVERIFY2(QFileInfo(iconPath).isFile(), qPrintable(iconPath));
#else
        QSKIP("macOS bundle test only");
#endif
    }
};

QTEST_GUILESS_MAIN(IconArtifactTest)

#include "IconArtifactTest.moc"
