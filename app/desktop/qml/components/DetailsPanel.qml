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
    readonly property int titleTextSize: Theme.densityValue(root.density, 14, 16, 18)
    readonly property int labelTextSize: Theme.densityValue(root.density, 12, 13, 14)
    readonly property int valueTextSize: Theme.densityValue(root.density, 12, 13, 15)
    signal openRequested
    // Emitted when the user clicks a relation node: the main view loads that
    // SSA into the details panel (not open SAM).
    signal loadRelationRequested(string ssaNumber)

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: Theme.gap

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: relationsLayout.implicitHeight
            visible: root.viewModel.relationCount > 0
            color: Theme.surface
            border.color: Theme.border
            radius: Theme.radius
            clip: true

            ColumnLayout {
                id: relationsLayout
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Item {
                        Layout.fillWidth: true
                    }

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

                Flow {
                    id: relationsFlow
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: root.viewModel.relations

                        delegate: Row {
                            id: relationRow
                            required property int index
                            required property var modelData
                            spacing: 6
                            height: relationBox.implicitHeight

                            Label {
                                visible: relationRow.index > 0
                                text: "->"
                                color: Theme.mutedText
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                id: relationBox
                                width: Math.max(Theme.relationNodeMinWidth, relationText.implicitWidth + 18)
                                implicitHeight: Theme.relationNodeHeight
                                radius: Theme.radius
                                color: relationRow.index === 0 ? Theme.accentSoft : Theme.panelRaised
                                border.color: relationRow.index === 0 ? Theme.accent : Theme.border

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 1

                                    Text {
                                        id: relationText
                                        text: relationRow.modelData.ssa
                                        color: Theme.text
                                        font.bold: true
                                        font.pixelSize: root.valueTextSize
                                    }

                                    Text {
                                        text: {
                                            const status = relationRow.modelData.status !== undefined ? relationRow.modelData.status : "";
                                            return status.length > 0 ? "<b>" + status + "</b> " + relationRow.modelData.kind : relationRow.modelData.kind;
                                        }
                                        color: Theme.mutedText
                                        font.pixelSize: Math.max(10, root.valueTextSize - 2)
                                        textFormat: Text.RichText
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (relationRow.index > 0) {
                                            root.loadRelationRequested(relationRow.modelData.ssa);
                                        }
                                    }
                                    ToolTip.visible: containsMouse
                                    ToolTip.delay: 0
                                    ToolTip.text: {
                                        const s = relationRow.modelData.ssa !== undefined ? relationRow.modelData.ssa : "";
                                        const st = relationRow.modelData.status !== undefined ? relationRow.modelData.status : "";
                                        const k = relationRow.modelData.kind !== undefined ? relationRow.modelData.kind : "";
                                        return s + (st.length > 0 ? " [" + st + "]" : "") + (k.length > 0 ? " - " + k : "");
                                    }
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
