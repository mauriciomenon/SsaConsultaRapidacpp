import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

Button {
    id: control
    implicitHeight: Theme.controlHeight
    padding: 8

    background: Rectangle {
        color: !control.enabled ? Theme.border : (control.down ? "#174f86" : Theme.accent)
        radius: Theme.radius
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? Theme.accentText : Theme.mutedText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
