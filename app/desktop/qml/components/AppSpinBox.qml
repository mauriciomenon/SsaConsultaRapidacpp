import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

SpinBox {
    id: root

    readonly property int reservedDigitCount: 3
    readonly property int horizontalTextPadding: 8
    readonly property int indicatorWidth: 27
    readonly property string reservedDigitProbe: "888"

    implicitHeight: Theme.controlHeight
    implicitWidth: Math.max(Theme.controlHeight * 2, Math.ceil(reservedDigitMetrics.width) + root.indicatorWidth + (root.horizontalTextPadding * 2))
    leftPadding: root.horizontalTextPadding
    rightPadding: root.indicatorWidth + root.horizontalTextPadding
    editable: false
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody

    TextMetrics {
        id: reservedDigitMetrics
        font: root.font
        text: root.reservedDigitProbe.substring(0, Math.max(0, root.reservedDigitCount))
    }

    contentItem: TextInput {
        z: 2
        text: root.textFromValue(root.value, root.locale)
        font: root.font
        color: root.enabled ? Theme.text : Theme.mutedText
        selectionColor: Theme.accentSoft
        selectedTextColor: Theme.text
        horizontalAlignment: Qt.AlignRight
        verticalAlignment: Qt.AlignVCenter
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        clip: true
    }

    up.indicator: Rectangle {
        x: root.width - width - 1
        y: 1
        width: root.indicatorWidth
        height: Math.floor((root.height - 2) / 2)
        color: root.up.pressed ? Theme.accentSoft : root.up.hovered ? Theme.surface : Theme.panelRaised
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
        width: root.indicatorWidth
        height: root.height - y - 1
        color: root.down.pressed ? Theme.accentSoft : root.down.hovered ? Theme.surface : Theme.panelRaised
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
