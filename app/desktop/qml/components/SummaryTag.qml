pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Control {
    id: root
    required property string text
    property string tooltipText: text
    property bool compact: false
    property int tagTextSize: 12
    property real preferredTagWidth: naturalWidth
    property color tagAccent: Theme.accent
    readonly property int naturalWidth: tagLabel.implicitWidth + removeButton.implicitWidth + Theme.chipChromePadding
    signal removeRequested

    Layout.minimumWidth: naturalWidth
    Layout.preferredWidth: preferredTagWidth
    implicitWidth: preferredTagWidth
    implicitHeight: root.compact ? Theme.chipHeightCompact : Theme.chipHeight
    padding: 0
    hoverEnabled: true

    ToolTip.visible: hovered && tooltipText.length > 0
    ToolTip.text: tooltipText
    ToolTip.delay: 0
    ToolTip.timeout: Theme.chipRemoveTooltipTimeoutMs

    background: Rectangle {
        color: Theme.surface
        border.color: root.tagAccent
        border.width: 1
        radius: Theme.radius
    }

    contentItem: RowLayout {
        spacing: Theme.spacingSm

        Label {
            id: tagLabel
            Layout.fillWidth: true
            Layout.leftMargin: Theme.chipLabelMargin
            text: root.text
            textFormat: Text.PlainText
            color: Theme.text
            font.pixelSize: Theme.fontSizeMicro
            font.bold: false
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        ToolButton {
            id: removeButton
            objectName: "summaryTagRemoveButton"
            readonly property color effectiveBackground: removeButton.hovered ? Theme.accentSoft : Theme.surface
            readonly property color effectiveForeground: Theme.readableText(effectiveBackground, Theme.accentStrong)
            Layout.preferredWidth: Theme.chipRemoveButtonSizeCompact
            Layout.preferredHeight: Theme.chipRemoveButtonSizeCompact
            text: "x"
            padding: 0
            font.pixelSize: root.tagTextSize
            font.bold: false
            palette.buttonText: effectiveForeground
            ToolTip.visible: hovered
            ToolTip.text: "Remover filtro"
            Accessible.name: "Remover filtro " + root.text
            onClicked: root.removeRequested()

            background: Rectangle {
                color: removeButton.hovered ? Theme.accentSoft : "transparent"
                radius: Theme.radius
            }
        }
    }
}
