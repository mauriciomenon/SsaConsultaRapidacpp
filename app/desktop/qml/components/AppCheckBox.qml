import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

CheckBox {
    id: root
    spacing: 8
    font.family: Theme.fontFamily
    font.pointSize: Theme.fontPointSizeBody
    font.weight: Font.Normal

    contentItem: Text {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        textFormat: Text.PlainText
        color: root.enabled ? Theme.text : Theme.mutedText
        font: root.font
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
