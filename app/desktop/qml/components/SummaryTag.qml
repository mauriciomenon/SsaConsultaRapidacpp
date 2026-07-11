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
            color: Theme.text
            font.pixelSize: root.tagTextSize
            font.bold: true
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        ToolButton {
            id: removeButton
            Layout.preferredWidth: root.compact ? Theme.chipRemoveButtonSizeCompact : Theme.chipRemoveButtonSize
            Layout.preferredHeight: root.compact ? Theme.chipRemoveButtonSizeCompact : Theme.chipRemoveButtonSize
            text: "x"
            padding: 0
            font.pixelSize: root.tagTextSize
            font.bold: true
            palette.buttonText: Theme.accentStrong
            ToolTip.visible: hovered
            ToolTip.text: "Remover filtro"
            onClicked: root.removeRequested()

            background: Rectangle {
                color: removeButton.hovered ? Theme.accentSoft : "transparent"
                radius: Theme.radius
            }
        }
    }
}
