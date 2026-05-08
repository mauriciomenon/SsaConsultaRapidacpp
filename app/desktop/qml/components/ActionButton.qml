import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

Button {
    id: control
    implicitHeight: Theme.controlHeight
    padding: 8

    background: Rectangle {
        color: control.down ? "#174f86" : Theme.accent
        radius: Theme.radius
    }

    contentItem: Text {
        text: control.text
        color: Theme.accentText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
