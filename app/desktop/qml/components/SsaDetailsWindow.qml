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
    property string graphExportMessage: ""
    property bool showMermaid: false
    property var navigationHistory: []
    property int navigationIndex: -1
    signal graphNodeRequested(string ssaNumber)
    signal copyTextRequested(string text)
    title: detailsViewModel && detailsViewModel.selectedSsaNumber.length > 0 ? "Detalhes da SSA " + detailsViewModel.selectedSsaNumber : "Detalhes da SSA"
    width: 1180
    height: 900
    minimumWidth: 880
    minimumHeight: 700
    visible: false
    color: Theme.window
    font.family: Theme.fontFamily

    palette.toolTipBase: Theme.panelRaised
    palette.toolTipText: Theme.text
    onClosing: root.destroy()

    function currentSsaNumber() {
        return root.detailsViewModel ? root.detailsViewModel.selectedSsaNumber : "";
    }

    function resetNavigationHistory() {
        const ssaNumber = root.currentSsaNumber();
        root.navigationHistory = ssaNumber.length > 0 ? [ssaNumber] : [];
        root.navigationIndex = root.navigationHistory.length - 1;
    }

    function navigateToSsa(ssaNumber) {
        if (!root.detailsViewModel || ssaNumber.length === 0)
            return;
        if (ssaNumber === root.currentSsaNumber()) {
            root.graphExportMessage = "SSA ja aberta";
            return;
        }
        if (!root.detailsViewModel.loadBySsaNumber(ssaNumber))
            return;
        const nextHistory = root.navigationHistory.slice(0, root.navigationIndex + 1);
        nextHistory.push(ssaNumber);
        root.navigationHistory = nextHistory;
        root.navigationIndex = root.navigationHistory.length - 1;
        root.graphExportMessage = "Historico: " + root.breadcrumbText();
    }

    function navigateHistory(offset) {
        root.navigateHistoryTo(root.navigationIndex + offset);
    }

    function navigateHistoryTo(nextIndex) {
        if (!root.detailsViewModel || nextIndex < 0 || nextIndex >= root.navigationHistory.length)
            return;
        const ssaNumber = root.navigationHistory[nextIndex];
        if (!root.detailsViewModel.loadBySsaNumber(ssaNumber))
            return;
        root.navigationIndex = nextIndex;
        root.graphExportMessage = "Historico: " + root.breadcrumbText();
    }

    function breadcrumbText() {
        if (root.navigationHistory.length === 0)
            return "";
        return root.navigationHistory.join(" / ");
    }

    onDetailsViewModelChanged: root.resetNavigationHistory()

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

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            ActionButton {
                text: "<"
                implicitWidth: 26
                implicitHeight: 24
                enabled: root.navigationIndex > 0
                ToolTip.visible: hovered
                ToolTip.text: "Voltar no historico"
                ToolTip.delay: 0
                onClicked: root.navigateHistory(-1)
            }

            ActionButton {
                text: ">"
                implicitWidth: 26
                implicitHeight: 24
                enabled: root.navigationIndex >= 0 && root.navigationIndex < root.navigationHistory.length - 1
                ToolTip.visible: hovered
                ToolTip.text: "Avancar no historico"
                ToolTip.delay: 0
                onClicked: root.navigateHistory(1)
            }

            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                clip: true
                contentWidth: historyRow.width
                contentHeight: historyRow.height
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick

                Row {
                    id: historyRow
                    spacing: 5
                    height: 26

                    Label {
                        text: root.navigationHistory.length > 0 ? "Historico " + (root.navigationIndex + 1) + "/" + root.navigationHistory.length : ""
                        color: Theme.mutedText
                        font.pixelSize: 12
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        height: 24
                    }

                    Repeater {
                        model: root.navigationHistory

                        delegate: Rectangle {
                            id: historyChip
                            required property int index
                            required property string modelData
                            width: Math.max(76, historyText.implicitWidth + 18)
                            height: 24
                            radius: Theme.radius
                            color: historyChip.index === root.navigationIndex ? Theme.accentSoft : Theme.surface
                            border.color: historyChip.index === root.navigationIndex ? Theme.accent : Theme.border

                            Text {
                                id: historyText
                                anchors.centerIn: parent
                                text: historyChip.modelData
                                color: historyChip.index === root.navigationIndex ? Theme.accentStrong : Theme.text
                                font.pixelSize: 12
                                font.bold: historyChip.index === root.navigationIndex
                                elide: Text.ElideRight
                                width: historyChip.width - 10
                                horizontalAlignment: Text.AlignHCenter
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.navigateHistoryTo(historyChip.index)
                            }
                        }
                    }
                }
            }
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
                SplitView.preferredHeight: 345
                SplitView.minimumHeight: 240
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
                            text: "Copiar"
                            implicitWidth: 70
                            implicitHeight: 26
                            enabled: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                            ToolTip.visible: hovered
                            ToolTip.text: "Copiar codigo Mermaid"
                            ToolTip.delay: 0
                            onClicked: root.copyTextRequested(root.detailsViewModel.graphModel.mermaid)
                        }

                        ActionButton {
                            text: "PNG"
                            implicitWidth: 54
                            implicitHeight: 26
                            enabled: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                            onClicked: {
                                root.showMermaid = false;
                                graphExportDialog.open();
                            }
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
                        visible: !root.showMermaid && root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                        graphModel: root.detailsViewModel ? root.detailsViewModel.graphModel : null
                        onNodeClicked: ssaNumber => root.navigateToSsa(ssaNumber)
                        onExportFinished: succeeded => root.graphExportMessage = succeeded ? "Grafo exportado" : "Falha ao exportar grafo"
                    }

                    TextArea {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.showMermaid && root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                        readOnly: true
                        wrapMode: TextEdit.NoWrap
                        text: root.detailsViewModel ? root.detailsViewModel.graphModel.mermaid : ""
                        color: Theme.text
                        selectedTextColor: Theme.accentText
                        selectionColor: Theme.accent
                        background: Rectangle {
                            color: Theme.surface
                            border.color: Theme.borderSoft
                            radius: Theme.radius
                        }
                    }

                    Label {
                        visible: root.detailsViewModel && root.detailsViewModel.graphModel.nodeCount > 0
                        text: root.graphExportMessage.length > 0 ? root.graphExportMessage : root.detailsViewModel ? root.detailsViewModel.graphModel.summary + " | Cheia: derivada | tracejada: relacionada | faixa: papel da SSA" : ""
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

                            TextEdit {
                                Layout.fillWidth: true
                                text: fieldDelegate.rowValue
                                color: Theme.text
                                readOnly: true
                                selectByMouse: true
                                selectedTextColor: Theme.accentText
                                selectionColor: Theme.accent
                                wrapMode: TextEdit.Wrap
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
