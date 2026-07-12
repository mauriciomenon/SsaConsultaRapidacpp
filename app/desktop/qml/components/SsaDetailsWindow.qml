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
    objectName: DesktopSmokeObjectNames.detailsWindow
    property var detailsViewModel: null
    property string graphStatusMessage: ""
    signal copyTextRequested(string text)
    title: detailsViewModel && detailsViewModel.selectedSsaNumber.length > 0 ? "Detalhes da SSA " + detailsViewModel.selectedSsaNumber : "Detalhes da SSA"
    width: Theme.clampedWindowDimension(Screen.desktopAvailableWidth, Theme.detailsWindowPreferredWidth, Theme.detailsWindowMinWidth)
    height: Theme.clampedWindowDimension(Screen.desktopAvailableHeight, Theme.detailsWindowPreferredHeight, Theme.detailsWindowMinHeight)
    minimumWidth: Theme.detailsWindowMinWidth
    minimumHeight: Theme.detailsWindowMinHeight
    visible: false
    color: Theme.window
    font.family: Theme.fontFamily

    palette.toolTipBase: Theme.panelRaised
    palette.toolTipText: Theme.text
    onClosing: root.destroy()

    function currentSsaNumber() {
        return root.detailsViewModel ? root.detailsViewModel.selectedSsaNumber : "";
    }

    function navigateToSsa(ssaNumber) {
        if (!root.detailsViewModel || ssaNumber.length === 0)
            return;
        if (ssaNumber === root.currentSsaNumber()) {
            root.graphStatusMessage = "SSA ja aberta";
            return;
        }
        root.graphStatusMessage = "";
        root.detailsViewModel.requestLoadBySsaNumber(ssaNumber);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: Theme.gap

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
                SplitView.preferredHeight: 290
                SplitView.minimumHeight: 210
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
                            font.pixelSize: Theme.fontSizeLabel
                        }

                        ActionButton {
                            text: "Copiar"
                            implicitWidth: 70
                            implicitHeight: 26
                            enabled: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                            ToolTip.visible: hovered
                            ToolTip.text: "Copiar diagrama SVG"
                            ToolTip.delay: 0
                            onClicked: root.copyTextRequested(root.detailsViewModel.graphModel.svg)
                        }

                        ActionButton {
                            text: "PNG"
                            implicitWidth: 54
                            implicitHeight: 26
                            enabled: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                            onClicked: graphExportDialog.open()
                        }
                    }

                    Label {
                        visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: root.detailsViewModel && root.detailsViewModel.relationLoading ? "Carregando detalhes" : root.detailsViewModel && root.detailsViewModel.relationError.length > 0 ? root.detailsViewModel.relationError : "Sem relacoes de derivacao para esta SSA"
                        textFormat: Text.PlainText
                        color: root.detailsViewModel && root.detailsViewModel.relationError.length > 0 ? Theme.danger : Theme.mutedText
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    DerivadasGraph {
                        id: derivadasGraph
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                        graphModel: root.detailsViewModel ? root.detailsViewModel.graphModel : null
                        onNodeClicked: ssaNumber => root.navigateToSsa(ssaNumber)
                        onExportFinished: succeeded => root.graphStatusMessage = succeeded ? "Grafo exportado" : "Falha ao exportar grafo"
                    }

                    Label {
                        visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                        text: root.detailsViewModel && root.detailsViewModel.relationLoading ? "Carregando detalhes" : root.detailsViewModel && root.detailsViewModel.relationError.length > 0 ? root.detailsViewModel.relationError : root.graphStatusMessage.length > 0 ? root.graphStatusMessage : root.detailsViewModel ? root.detailsViewModel.graphModel.summary + " | Cheia: derivada | tracejada: relacionada | faixa: papel da SSA" : ""
                        textFormat: Text.PlainText
                        color: root.detailsViewModel && root.detailsViewModel.relationError.length > 0 ? Theme.danger : Theme.mutedText
                        font.pixelSize: Theme.fontSizeMicro
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
                                font.pixelSize: Theme.fontSizeLabel
                                font.bold: false
                                text: fieldDelegate.label + ":"
                                color: Theme.text
                                elide: Text.ElideRight
                            }

                            TextEdit {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(18, contentHeight)
                                text: fieldDelegate.rowValue
                                textFormat: TextEdit.PlainText
                                color: Theme.text
                                readOnly: true
                                selectByMouse: true
                                selectedTextColor: Theme.accentText
                                selectionColor: Theme.accent
                                wrapMode: TextEdit.Wrap
                                font.pixelSize: Theme.fontSizeLabel
                                font.bold: false
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
