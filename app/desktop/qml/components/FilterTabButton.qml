import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

TabButton {
    id: root

    background: Rectangle {
        color: root.checked ? Theme.accent : Theme.panelRaised
        border.color: Theme.border
    }

    contentItem: Text {
        text: root.text
        color: root.checked ? Theme.readableText(Theme.accent, Theme.accentText) : Theme.readableText(Theme.panelRaised, Theme.text)
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
