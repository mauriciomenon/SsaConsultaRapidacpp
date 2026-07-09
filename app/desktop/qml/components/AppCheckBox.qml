import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

CheckBox {
    id: root
    spacing: 8

    contentItem: Text {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: root.enabled ? Theme.text : Theme.mutedText
        font.pixelSize: Theme.fontSizeMicro
        font.weight: Font.Normal
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Rectangle {
        x: 0
        y: (root.height - 16) / 2
        width: 16
        height: 16
        radius: 4
        border.color: Theme.border
        border.width: 1
        color: root.checked ? Theme.accent : Theme.panel

        Rectangle {
            visible: root.checked
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 2
            color: Theme.accentText
        }
    }
}
