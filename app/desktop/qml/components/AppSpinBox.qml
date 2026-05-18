import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

SpinBox {
    id: root

    implicitHeight: Theme.controlHeight
    font.family: Theme.fontFamily
    font.pixelSize: 12

    contentItem: TextInput {
        z: 2
        text: root.textFromValue(root.value, root.locale)
        font: root.font
        color: root.enabled ? Theme.text : Theme.mutedText
        selectionColor: Theme.accentSoft
        selectedTextColor: Theme.text
        leftPadding: 8
        rightPadding: 32
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    up.indicator: Rectangle {
        x: root.width - width - 1
        y: 1
        width: 27
        height: Math.floor((root.height - 2) / 2)
        color: root.up.pressed ? Theme.accentSoft
             : root.up.hovered ? Theme.surface
             : Theme.panelRaised
        border.color: Theme.border

        Text {
            anchors.centerIn: parent
            text: "^"
            color: Theme.mutedText
            font.bold: true
        }
    }

    down.indicator: Rectangle {
        x: root.width - width - 1
        y: Math.floor(root.height / 2)
        width: 27
        height: root.height - y - 1
        color: root.down.pressed ? Theme.accentSoft
             : root.down.hovered ? Theme.surface
             : Theme.panelRaised
        border.color: Theme.border

        Text {
            anchors.centerIn: parent
            text: "v"
            color: Theme.mutedText
            font.bold: true
        }
    }

    background: Rectangle {
        color: root.enabled ? Theme.panelRaised : Theme.rowAlt
        border.color: root.activeFocus ? Theme.accent : Theme.border
        radius: Theme.radius
    }
}
