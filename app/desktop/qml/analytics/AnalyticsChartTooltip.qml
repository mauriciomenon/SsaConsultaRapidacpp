import QtQuick
import SsaConsultaRapida

Rectangle {
    id: root

    property string text: ""
    property real pointerX: 0
    property real pointerY: 0

    readonly property real preferredX: pointerX + Theme.gap
    readonly property real preferredY: pointerY + Theme.gap

    x: Math.max(0, Math.min(parent ? parent.width - width : preferredX, preferredX))
    y: Math.max(0, Math.min(parent ? parent.height - height : preferredY, preferredY))
    width: Math.min(parent ? parent.width : tooltipText.implicitWidth + Theme.gap * 2, tooltipText.implicitWidth + Theme.gap * 2)
    height: tooltipText.implicitHeight + Theme.gap * 2
    visible: text.length > 0
    color: Theme.panelRaised
    border.color: Theme.accent
    radius: Theme.radius
    z: 10
    Accessible.role: Accessible.StaticText
    Accessible.name: text

    Text {
        id: tooltipText

        anchors.fill: parent
        anchors.margins: Theme.gap
        text: root.text
        textFormat: Text.PlainText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeCaption
        wrapMode: Text.Wrap
    }
}
