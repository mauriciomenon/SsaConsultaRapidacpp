pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SsaConsultaRapida

// Dedicated details window opened on double-click: shows the derivadas
// relation graph and the SSA fields. Mirrors the Python details_dialog.
ApplicationWindow {
    id: root
    property var detailsViewModel: null
    property string graphExportMessage: ""
    signal graphNodeRequested(string ssaNumber)
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

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical
            handle: Rectangle {
                implicitWidth: 7
                implicitHeight: 7
                color: SplitHandle.pressed ? Theme.accent : SplitHandle.hovered ? Theme.accentSoft : Theme.borderSoft
                border.color: SplitHandle.pressed || SplitHandle.hovered ? Theme.accentStrong : Theme.border
                border.width: 1
            }

            Rectangle {
                SplitView.preferredHeight: 220
                SplitView.minimumHeight: 160
                color: Theme.panel
                border.color: Theme.border
                radius: Theme.radius
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            Layout.fillWidth: true
                            text: "Grafo de derivadas"
                            color: Theme.accentStrong
                            font.bold: true
                            font.pixelSize: 13
                        }

                        ActionButton {
                            text: "Exportar PNG"
                            implicitWidth: 112
                            implicitHeight: 26
                            enabled: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                            onClicked: graphExportDialog.open()
                        }
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
                        id: derivadasGraph
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                        graphModel: root.detailsViewModel ? root.detailsViewModel.graphModel : null
                        onNodeClicked: ssaNumber => root.graphNodeRequested(ssaNumber)
                        onExportFinished: succeeded => root.graphExportMessage = succeeded ? "Grafo exportado" : "Falha ao exportar grafo"
                    }

                    Label {
                        visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                        text: root.graphExportMessage.length > 0 ? root.graphExportMessage : root.detailsViewModel ? root.detailsViewModel.graphModel.summary : ""
                        color: Theme.mutedText
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 180
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

    FileDialog {
        id: graphExportDialog
        title: "Exportar grafo de derivadas"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG (*.png)"]
        onAccepted: derivadasGraph.savePng(graphExportDialog.selectedFile)
    }
}
