import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

TextField {
    id: root

    implicitHeight: Theme.controlHeight
    leftPadding: 10
    rightPadding: 10
    color: Theme.text
    placeholderTextColor: Theme.mutedText
    selectionColor: Theme.accentSoft
    selectedTextColor: Theme.text
    background: Rectangle {
        color: Theme.panelRaised
        border.color: root.activeFocus ? Theme.accent : Theme.border
        border.width: 1
        radius: Theme.radius
    }
}
