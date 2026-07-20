import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

TextField {
    id: root

    readonly property real smokeAvailableWidth: width - leftPadding - rightPadding
    readonly property real smokeAvailableHeight: height - topPadding - bottomPadding
    readonly property real smokeContentWidth: smokeTextMetrics.width
    readonly property real smokeContentHeight: smokeTextMetrics.height

    implicitHeight: Theme.controlHeight
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody
    leftPadding: 10
    rightPadding: 10
    color: Theme.text
    placeholderTextColor: Theme.mutedText
    selectionColor: Theme.accentSoft
    selectedTextColor: Theme.text
    TextMetrics {
        id: smokeTextMetrics
        font: root.font
        text: root.text.length > 0 ? root.text : root.placeholderText
    }
    background: Rectangle {
        color: Theme.panelRaised
        border.color: root.activeFocus ? Theme.accent : Theme.border
        border.width: 1
        radius: Theme.radius
    }
}
