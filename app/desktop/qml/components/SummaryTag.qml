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
    signal removeRequested

    implicitWidth: Math.min(700, tagLabel.implicitWidth + removeButton.implicitWidth + 26)
    implicitHeight: compact ? 24 : 26
    padding: 0
    hoverEnabled: true

    ToolTip.visible: hovered && tooltipText.length > 0
    ToolTip.text: tooltipText
    ToolTip.delay: 0
    ToolTip.timeout: 10000

    background: Rectangle {
        color: Theme.surface
        border.color: Theme.accent
        radius: Theme.radius
    }

    contentItem: RowLayout {
        spacing: 4

        Label {
            id: tagLabel
            Layout.fillWidth: true
            Layout.leftMargin: 8
            text: root.text
            color: Theme.text
            font.pixelSize: root.tagTextSize
            font.bold: true
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        ToolButton {
            id: removeButton
            Layout.preferredWidth: root.compact ? 22 : 24
            Layout.preferredHeight: root.compact ? 22 : 24
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
