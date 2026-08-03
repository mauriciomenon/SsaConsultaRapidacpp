pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Dialog {
    id: root

    required property var workflowViewModel
    property string operationLabel: ""
    property string statusText: ""
    property string currentFileName: ""
    property int currentFile: 0
    property int totalFiles: 0
    property int percentage: 0
    property bool cancelRequested: false
    property bool terminal: false
    property bool terminalSucceeded: false
    property bool terminalCanceled: false
    property string terminalLabel: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: false
    closePolicy: Popup.NoAutoClose
    width: Math.min(800, parent.width - 32)
    height: Math.min(620, parent.height - 32)
    padding: 16
    title: (terminal ? terminalLabel : operationLabel) + (totalFiles > 0 ? " - " + currentFile + "/" + totalFiles : "")

    function appendLine(area, scrollBar, line) {
        if (line.length === 0)
            return;
        area.append(line);
        Qt.callLater(function () {
            scrollBar.position = Math.max(0, 1 - scrollBar.size);
        });
    }

    function beginSession(label) {
        operationLabel = label;
        statusText = label;
        currentFileName = "";
        currentFile = 0;
        totalFiles = 0;
        percentage = 0;
        cancelRequested = false;
        terminal = false;
        terminalSucceeded = false;
        terminalCanceled = false;
        terminalLabel = "";
        outputArea.text = "";
        errorArea.text = "";
        open();
    }

    background: Rectangle {
        color: Theme.panelRaised
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius
    }

    header: Label {
        objectName: "workflowProgressTitle"
        text: root.title
        color: Theme.text
        font.bold: true
        font.pixelSize: Theme.fontSizeTitle
        padding: 16
        bottomPadding: 8
        elide: Text.ElideRight
    }

    contentItem: ColumnLayout {
        spacing: Theme.gap

        Label {
            id: statusLabel
            objectName: "workflowProgressStatus"
            Layout.fillWidth: true
            text: root.statusText
            color: Theme.text
            font.pixelSize: Theme.fontSizeBody
            wrapMode: Text.WordWrap
        }

        Label {
            objectName: "workflowProgressFile"
            Layout.fillWidth: true
            text: root.currentFileName
            color: Theme.mutedText
            font.pixelSize: Theme.fontSizeLabel
            elide: Text.ElideMiddle
            visible: text.length > 0
        }

        ProgressBar {
            objectName: "workflowProgressBar"
            Layout.fillWidth: true
            from: 0
            to: 100
            value: root.percentage
        }

        Label {
            objectName: "workflowProgressOutputLabel"
            text: "Saida da execucao atual"
            color: Theme.text
            font.bold: true
            font.pixelSize: Theme.fontSizeLabel
        }

        ScrollView {
            objectName: "workflowProgressOutputScroll"
            implicitHeight: 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            Layout.preferredHeight: 120

            ScrollBar.vertical: ScrollBar {
                id: outputScrollBar
                objectName: "workflowProgressOutputScrollBar"
            }

            TextArea {
                id: outputArea
                objectName: "workflowProgressOutput"
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                background: Rectangle {
                    color: Theme.panel
                    border.color: Theme.borderSoft
                    border.width: 1
                    radius: Theme.radius
                }
            }
        }

        Label {
            objectName: "workflowProgressErrorLabel"
            text: root.terminal && !root.terminalSucceeded && !root.terminalCanceled ? "Erros" : "Avisos"
            color: Theme.text
            font.bold: true
            font.pixelSize: Theme.fontSizeLabel
        }

        ScrollView {
            objectName: "workflowProgressErrorScroll"
            implicitHeight: 0
            Layout.fillWidth: true
            Layout.minimumHeight: 100
            Layout.preferredHeight: 100

            ScrollBar.vertical: ScrollBar {
                id: errorScrollBar
            }

            TextArea {
                id: errorArea
                objectName: "workflowProgressErrors"
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: root.terminal && !root.terminalSucceeded && !root.terminalCanceled ? Theme.danger : Theme.mutedText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                background: Rectangle {
                    color: Theme.panel
                    border.color: Theme.borderSoft
                    border.width: 1
                    radius: Theme.radius
                }
            }
        }
    }

    footer: Item {
        implicitHeight: cancelButton.implicitHeight + 16

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: Theme.gap

            ActionButton {
                id: cancelButton
                objectName: "workflowProgressCancelButton"
                text: root.cancelRequested ? "Cancelamento solicitado" : "Cancelar"
                enabled: !root.terminal && !root.cancelRequested && root.workflowViewModel.canCancel
                implicitWidth: 180
                onClicked: {
                    if (root.terminal || root.cancelRequested)
                        return;
                    root.cancelRequested = true;
                    root.statusText = "Cancelamento solicitado";
                    root.workflowViewModel.cancel();
                }
            }

            Item {
                Layout.fillWidth: true
            }

            ActionButton {
                id: closeButton
                objectName: "workflowProgressCloseButton"
                text: "Fechar"
                enabled: root.terminal
                implicitWidth: 100
                onClicked: root.close()
            }
        }
    }

    Connections {
        target: root.workflowViewModel

        function onProgressSessionStarted(operationLabel) {
            root.beginSession(operationLabel);
        }

        function onProgressChanged(percentage, status, currentFile, totalFiles, fileName) {
            root.percentage = percentage;
            root.currentFile = currentFile;
            root.totalFiles = totalFiles;
            root.currentFileName = fileName;
            if (!root.cancelRequested)
                root.statusText = status;
        }

        function onProgressOutputLine(line) {
            root.appendLine(outputArea, outputScrollBar, line);
        }

        function onProgressErrorLine(line) {
            root.appendLine(errorArea, errorScrollBar, line);
        }

        function onProgressSessionFinished(succeeded, canceled, title, message) {
            root.terminal = true;
            root.terminalSucceeded = succeeded;
            root.terminalCanceled = canceled;
            root.terminalLabel = title;
            root.percentage = succeeded ? 100 : root.percentage;
            root.statusText = message;
        }
    }
}
