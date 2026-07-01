pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

// Dedicated details window opened on double-click: shows the derivadas
// relation graph and the SSA fields. Mirrors the Python details_dialog.
ApplicationWindow {
    id: root
    property var detailsViewModel: null
    title: detailsViewModel && detailsViewModel.selectedSsaNumber.length > 0 ? "Detalhes da SSA " + detailsViewModel.selectedSsaNumber : "Detalhes da SSA"
    width: 760
    height: 620
    minimumWidth: 520
    minimumHeight: 420
    visible: false
    color: Theme.window
    font.family: Theme.fontFamily

    palette.toolTipBase: Theme.panelRaised
    palette.toolTipText: Theme.text

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            text: root.detailsViewModel ? root.detailsViewModel.title : ""
            color: Theme.text
            font.bold: true
            font.pixelSize: 16
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            color: Theme.panel
            border.color: Theme.border
            radius: Theme.radius
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Label {
                    text: "Grafo de derivadas"
                    color: Theme.accentStrong
                    font.bold: true
                    font.pixelSize: 13
                }

                Label {
                    visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Sem relacoes de derivacao para esta SSA"
                    color: Theme.mutedText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                DerivadasGraph {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                    graphModel: root.detailsViewModel ? root.detailsViewModel.graphModel : null
                }

                Label {
                    visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                    text: root.detailsViewModel ? root.detailsViewModel.graphModel.summary : ""
                    color: Theme.mutedText
                    font.pixelSize: 11
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.panel
            border.color: Theme.border
            radius: Theme.radius
            clip: true

            ListView {
                id: detailsList
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                interactive: true
                model: root.detailsViewModel ? root.detailsViewModel.fields : null
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
                            font.pixelSize: 13
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
                            font.pixelSize: 13
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
                anchors.fill: parent
                anchors.margins: 10
                visible: root.detailsViewModel && root.detailsViewModel.fieldCount === 0
                text: "Selecione uma linha da tabela para ver os detalhes"
                color: Theme.mutedText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
            }
        }
    }
}
