pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    required property var browseViewModel
    property string density: "normal"
    readonly property int labelTextSize: Theme.densityValue(root.density, 12, 13, 14)
    readonly property int valueTextSize: Theme.densityValue(root.density, 12, 13, 15)
    readonly property int nodeTextSize: Math.max(10, Theme.densityValue(root.density, 10, 11, 12))
    signal openRequested
    // Emitted when the user clicks a relation node: the main view should load
    // that SSA into the details panel (not open SAM).
    signal loadRelationRequested(string ssaNumber)

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: Theme.gap

        // 2b/2c: compact nav buttons in a small box, top-right corner.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.preferredHeight: 26
                Layout.preferredWidth: navRow.implicitWidth + 8
                color: Theme.panelRaised
                border.color: Theme.border
                radius: Theme.radius

                RowLayout {
                    id: navRow
                    anchors.centerIn: parent
                    spacing: 2

                    ActionButton {
                        text: "<"
                        implicitWidth: 22
                        implicitHeight: 22
                        enabled: root.browseViewModel.canSelectPreviousRow
                        onClicked: root.browseViewModel.selectPreviousRow()
                    }
                    ActionButton {
                        text: ">"
                        implicitWidth: 22
                        implicitHeight: 22
                        enabled: root.browseViewModel.canSelectNextRow
                        onClicked: root.browseViewModel.selectNextRow()
                    }
                }
            }
        }

        // 2d/2j: relation flow (mae -> atual -> filhas), horizontal, compact.
        // 2e: shows status only (no "Atual" word). 2f: Der./Rel. indicator.
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            clip: true
            ScrollBar.horizontal.policy: summaryFlow.width > width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff

            Row {
                id: summaryFlow
                spacing: 4
                height: 48

                Repeater {
                    model: root.viewModel.relations

                    delegate: Rectangle {
                        id: relNode
                        required property int index
                        required property var modelData
                        readonly property bool isCurrent: modelData.kind === "Atual"
                        readonly property string shortKind: {
                            const k = modelData.kind;
                            if (k === "Derivada" || k === "Derivada de")
                                return "Der.";
                            if (k === "Relacionada")
                                return "Rel.";
                            return "";
                        }
                        width: Math.max(64, relColumn.implicitWidth + 12)
                        height: 48
                        radius: Theme.radius
                        color: isCurrent ? Theme.accentSoft : Theme.panelRaised
                        border.color: isCurrent ? Theme.accent : Theme.border
                        border.width: isCurrent ? 2 : 1

                        ToolTip.visible: relArea.containsMouse
                        ToolTip.delay: 0
                        ToolTip.text: {
                            const s = modelData.ssa !== undefined ? modelData.ssa : "";
                            const st = modelData.status !== undefined ? modelData.status : "";
                            const k = modelData.kind !== undefined ? modelData.kind : "";
                            return s + (st.length > 0 ? " [" + st + "]" : "") + (k.length > 0 ? " - " + k : "");
                        }

                        ColumnLayout {
                            id: relColumn
                            anchors.centerIn: parent
                            spacing: 0

                            Text {
                                text: relNode.modelData.ssa !== undefined ? relNode.modelData.ssa : ""
                                color: Theme.text
                                font.bold: true
                                font.pixelSize: root.nodeTextSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                visible: relNode.modelData.status !== undefined && relNode.modelData.status.length > 0
                                text: "<b>" + (relNode.modelData.status !== undefined ? relNode.modelData.status : "") + "</b>"
                                color: Theme.mutedText
                                font.pixelSize: Math.max(9, root.nodeTextSize - 1)
                                textFormat: Text.RichText
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                visible: relNode.shortKind.length > 0
                                text: relNode.shortKind
                                color: Theme.mutedText
                                font.pixelSize: Math.max(9, root.nodeTextSize - 1)
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: relArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!relNode.isCurrent && relNode.modelData.ssa !== undefined) {
                                    root.loadRelationRequested(relNode.modelData.ssa);
                                }
                            }
                        }
                    }
                }
            }
        }

        ListView {
            id: detailsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fieldCount > 0
            clip: true
            interactive: true
            model: root.viewModel.fields
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                id: fieldDelegate
                required property string label
                required property var value
                property string rowValue: value === undefined || value === null ? "" : String(value)

                width: detailsList.width
                spacing: 2

                RowLayout {
                    width: parent.width
                    spacing: Theme.gap

                    Label {
                        Layout.preferredWidth: Theme.detailsLabelWidth
                        font.pixelSize: root.labelTextSize
                        font.bold: true
                        text: fieldDelegate.label + ":"
                        color: Theme.text
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: fieldDelegate.rowValue
                        color: Theme.text
                        wrapMode: Text.Wrap
                        font.pixelSize: root.valueTextSize
                        font.bold: true
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                    opacity: 0.8
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fieldCount === 0
            text: "Selecione uma linha da tabela para ver os detalhes"
            color: Theme.mutedText
            font.pixelSize: root.valueTextSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
