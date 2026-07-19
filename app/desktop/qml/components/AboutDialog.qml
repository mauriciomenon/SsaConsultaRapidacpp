pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SsaConsultaRapida

Window {
    id: root

    readonly property string productName: "SSA Consulta Rapida"
    readonly property string authorName: "Mauricio Menon"
    readonly property string productVersion: Qt.application.version.length > 0 ? Qt.application.version : "0.0.0"

    title: "Sobre"
    modality: Qt.ApplicationModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
    color: Theme.window
    minimumWidth: Math.min(420, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(260, Screen.desktopAvailableHeight)
    width: Math.min(520, Screen.desktopAvailableWidth)
    height: Math.min(320, Screen.desktopAvailableHeight)

    function open() {
        root.show();
        root.raise();
        root.requestActivate();
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: Theme.cardGap

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.productName
                color: Theme.text
                font.bold: true
                font.pixelSize: Theme.fontSizeTitle
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Versao " + root.productVersion
                color: Theme.readableText(Theme.panel, Theme.accent)
                font.pixelSize: Theme.fontSizeBody
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Autor: " + root.authorName
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
            }
            Item {
                Layout.fillHeight: true
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: "Fechar"
                onClicked: root.close()
            }
        }
    }
}
