pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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
    property url logFileUrl
    // Feedback of the report menu; kept apart from statusText, which is part of the report.
    property string reportFeedback: ""
    // The text areas keep only the newest lines; the report keeps every line.
    property var fullOutput: []
    property var fullErrors: []

    signal copyTextRequested(string text)

    function reportText() {
        return root.title + "\n" + root.statusText + "\n\nSaida da execucao\n" + root.fullOutput.join("\n") + "\n\n" + errorLabel.text + "\n" + root.fullErrors.join("\n") + "\n";
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: false
    closePolicy: Popup.NoAutoClose
    width: Math.min(800, parent.width - 32)
    height: Math.min(620, parent.height - 32)
    padding: 16
    title: (terminal ? terminalLabel : operationLabel) + (totalFiles > 0 ? " - " + currentFile + "/" + totalFiles : "")

    // Python parity (rescan_progress_dialog MAX_OUTPUT_BLOCKS): keep the newest 4000 lines.
    readonly property int maxLogLines: 4000
    readonly property int logTrimBatch: 500

    function appendLine(area, scrollBar, rawLine) {
        if (rawLine.length === 0)
            return;
        // Qt breaks lines on CR and U+2028/U+2029 too; use LF so every break is counted.
        const line = rawLine.replace(/\r\n?|[\u2028\u2029]/g, "\n");
        area.append(line);
        area.lineTotal += line.split("\n").length;
        // Trimming in batches keeps the cost of copying the text amortized.
        if (area.lineTotal > root.maxLogLines + root.logTrimBatch) {
            const text = area.text;
            let position = -1;
            for (let index = root.maxLogLines; index < area.lineTotal; ++index)
                position = text.indexOf("\n", position + 1);
            area.remove(0, position + 1);
            area.lineTotal = root.maxLogLines;
        }
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
        outputArea.lineTotal = 0;
        errorArea.text = "";
        errorArea.lineTotal = 0;
        reportFeedback = "";
        fullOutput = [];
        fullErrors = [];
        open();
    }

    background: Rectangle {
        color: Theme.panelRaised
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius
    }

    header: RowLayout {
        spacing: Theme.gap

        Label {
            objectName: "workflowProgressTitle"
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.bold: true
            font.pixelSize: Theme.fontSizeTitle
            padding: 16
            rightPadding: 0
            bottomPadding: 8
            elide: Text.ElideRight
        }

        ToolButton {
            id: reportMenuButton
            objectName: "workflowProgressReportButton"
            Layout.rightMargin: 16
            Layout.topMargin: 8
            Layout.alignment: Qt.AlignTop
            implicitWidth: 22
            implicitHeight: 22
            text: "..."
            padding: 0
            font.pixelSize: Theme.fontSizeMicro
            ToolTip.visible: hovered
            ToolTip.text: "Relatorio e log"
            Accessible.name: "Relatorio e log"
            onClicked: reportMenu.open()

            background: Rectangle {
                color: reportMenuButton.hovered ? Theme.accentSoft : Theme.panelRaised
                border.color: Theme.border
                radius: Theme.radius
            }

            Menu {
                id: reportMenu
                objectName: "workflowProgressReportMenu"
                x: reportMenuButton.width - width
                y: reportMenuButton.height

                MenuItem {
                    objectName: "workflowProgressCopyReport"
                    text: "Copiar relatorio"
                    enabled: outputArea.length > 0 || errorArea.length > 0
                    onTriggered: root.copyTextRequested(root.reportText())
                }
                MenuItem {
                    objectName: "workflowProgressSaveReport"
                    text: "Salvar relatorio (.txt)..."
                    enabled: outputArea.length > 0 || errorArea.length > 0
                    onTriggered: reportSaveDialog.open()
                }
                MenuSeparator {}
                MenuItem {
                    objectName: "workflowProgressOpenLog"
                    text: "Abrir arquivo de log"
                    enabled: root.logFileUrl.toString().length > 0
                    onTriggered: Qt.openUrlExternally(root.logFileUrl)
                }
            }
        }
    }

    FileDialog {
        id: reportSaveDialog
        title: "Salvar relatorio"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Texto (*.txt)"]
        onAccepted: root.reportFeedback = root.workflowViewModel.saveReport(selectedFile, root.reportText()) ? "Relatorio salvo" : "Falha ao salvar o relatorio"
    }

    contentItem: ColumnLayout {
        spacing: Theme.gap

        Label {
            id: statusLabel
            objectName: "workflowProgressStatus"
            Layout.fillWidth: true
            text: root.reportFeedback.length > 0 ? root.reportFeedback : root.statusText
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
                property int lineTotal: 0
                textFormat: TextEdit.PlainText
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
            id: errorLabel
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
                property int lineTotal: 0
                textFormat: TextEdit.PlainText
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
            root.fullOutput.push(line);
            root.appendLine(outputArea, outputScrollBar, line);
        }

        function onProgressErrorLine(line) {
            root.fullErrors.push(line);
            root.appendLine(errorArea, errorScrollBar, line);
        }

        function onProgressSessionFinished(succeeded, canceled, title, message) {
            root.terminal = true;
            root.terminalSucceeded = succeeded;
            root.terminalCanceled = canceled;
            root.terminalLabel = title;
            root.percentage = succeeded ? 100 : root.percentage;
            root.statusText = message;
            root.reportFeedback = "";
        }
    }
}
