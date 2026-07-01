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
    readonly property int detailsLabelWidth: Theme.densityValue(root.density, 118, 132, 150)
    signal openRequested
    signal graphWindowRequested
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

                Flickable {
                    id: relationsFlick
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.relationNodeHeight + 6
                    clip: true
                    contentWidth: relationsRow.width
                    contentHeight: relationsRow.height
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.HorizontalFlick

                    Row {
                        id: relationsRow
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
                                    color: relationRow.index === 0 ? Theme.accentSoft : relationRow.modelData.role === "related" ? Theme.surface : Theme.panelRaised
                                    border.color: relationRow.index === 0 ? Theme.accent : relationRow.modelData.role === "related" ? Theme.link : Theme.border

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 1

                                        Text {
                                            id: relationText
                                            text: relationRow.modelData.ssa
                                            // The Current node sits on accentSoft. Pick the foreground that
                                            // contrasts with that specific tint across all themes.
                                            color: relationRow.index === 0 ? (Theme.isDarkTint(Theme.accentSoft) ? Theme.text : (Theme.dark ? Theme.accentText : Theme.text)) : Theme.text
                                            font.bold: true
                                            font.pixelSize: root.valueTextSize
                                        }

                                        Text {
                                            text: {
                                                const status = relationRow.modelData.status !== undefined ? relationRow.modelData.status : "";
                                                const kind = relationRow.modelData.kind !== undefined ? relationRow.modelData.kind : "";
                                                if (status.length > 0 && kind.length > 0)
                                                    return "<b>" + status + "</b> " + kind;
                                                if (status.length > 0)
                                                    return "<b>" + status + "</b>";
                                                return kind;
                                            }
                                            color: relationRow.index === 0 ? (Theme.isDarkTint(Theme.accentSoft) ? Theme.mutedText : (Theme.dark ? Theme.accentText : Theme.text)) : Theme.mutedText
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

                        // Navigation buttons aligned with the relation row.
                        Row {
                            spacing: 2
                            height: Theme.relationNodeHeight
                            leftPadding: 6

                            ActionButton {
                                text: "<"
                                implicitWidth: 22
                                implicitHeight: 22
                                anchors.verticalCenter: parent.verticalCenter
                                enabled: root.viewModel.canSelectPreviousRelation
                                onClicked: root.viewModel.selectPreviousRelation()
                            }
                            Label {
                                text: root.viewModel.relationCount > 0 ? (root.viewModel.currentRelationIndex + 1) + "/" + root.viewModel.relationCount : ""
                                color: Theme.mutedText
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                width: 32
                                height: 22
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            ActionButton {
                                text: ">"
                                implicitWidth: 22
                                implicitHeight: 22
                                anchors.verticalCenter: parent.verticalCenter
                                enabled: root.viewModel.canSelectNextRelation
                                onClicked: root.viewModel.selectNextRelation()
                            }
                            ActionButton {
                                text: "Grafo"
                                implicitWidth: 54
                                implicitHeight: 22
                                anchors.verticalCenter: parent.verticalCenter
                                enabled: root.viewModel.selectedSsaNumber.length > 0
                                onClicked: root.graphWindowRequested()
                            }
                        }
                    }

                    // Thin themed scrollbar only when content overflows.
                    ScrollBar.horizontal: ScrollBar {
                        policy: relationsFlick.contentWidth > relationsFlick.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        contentItem: Rectangle {
                            implicitHeight: 2
                            color: Theme.accent
                            opacity: 0.7
                            radius: 1
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
                spacing: 0

                RowLayout {
                    width: parent.width
                    spacing: 4

                    Label {
                        Layout.preferredWidth: root.detailsLabelWidth
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
