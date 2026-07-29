pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SsaConsultaRapida

Dialog {
    id: root
    objectName: "dataSetupWizard"

    required property var viewModel
    property bool forceRequired: false
    property string sourceDatabaseSummary: "Nenhum banco selecionado"
    property string xlsxSummary: "Nenhuma planilha selecionada"
    readonly property bool requiresSourceDatabase: viewModel.action === 1 || viewModel.action === 3
    readonly property bool requiresXlsx: viewModel.action === 2 || viewModel.action === 3

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: forceRequired || viewModel.running ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(760, parent.width - 32)
    height: Math.min(560, parent.height - 32)
    padding: 16
    title: "Configurar dados"

    function actionTitle(action) {
        switch (action) {
        case 0:
            return "Criar banco vazio";
        case 1:
            return "Importar banco existente";
        case 2:
            return "Importar planilhas XLSX";
        case 3:
            return "Importar banco e planilhas XLSX";
        default:
            return "";
        }
    }

    function resetNavigation() {
        wizardStack.clear();
        wizardStack.push(actionPage);
    }

    function openForced() {
        forceRequired = true;
        resetNavigation();
        open();
    }

    function openOptional() {
        forceRequired = false;
        resetNavigation();
        open();
    }

    background: Rectangle {
        color: Theme.panelRaised
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius
    }

    header: Label {
        text: root.title
        color: Theme.text
        font.family: Theme.fontFamily
        font.bold: true
        font.pixelSize: Theme.fontSizeTitle
        padding: 16
        bottomPadding: 8
    }

    contentItem: Item {
        StackView {
            id: wizardStack
            anchors.fill: parent
            clip: true
            initialItem: actionPage
            enabled: !root.viewModel.running
        }

        Rectangle {
            id: progressOverlay
            objectName: "progressOverlay"
            anchors.fill: parent
            z: 10
            visible: root.viewModel.running
            color: Theme.panelRaised
            border.color: Theme.border
            radius: Theme.radius
            focus: visible

            onVisibleChanged: {
                if (visible)
                    cancelDataSetupButton.forceActiveFocus();
            }

            MouseArea {
                anchors.fill: parent
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(420, parent.width - 32)
                spacing: Theme.gap

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: progressOverlay.visible
                }

                Label {
                    Layout.fillWidth: true
                    text: root.viewModel.progressMessage.length > 0 ? root.viewModel.progressMessage : "Configurando dados..."
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                ActionButton {
                    id: cancelDataSetupButton
                    Layout.alignment: Qt.AlignHCenter
                    text: "Cancelar operacao"
                    enabled: root.viewModel.canCancel
                    implicitWidth: 170
                    onClicked: root.viewModel.cancel()
                }
            }
        }
    }

    footer: RowLayout {
        spacing: Theme.gap
        enabled: !root.viewModel.running

        ActionButton {
            text: "Voltar"
            visible: wizardStack.depth > 1
            onClicked: wizardStack.pop()
        }

        Item {
            Layout.fillWidth: true
        }

        ActionButton {
            text: "Cancelar"
            visible: !root.forceRequired
            onClicked: root.close()
        }

        ActionButton {
            text: "Avancar"
            visible: wizardStack.depth < 3
            onClicked: wizardStack.push(wizardStack.depth === 1 ? destinationPage : summaryPage)
        }

        ActionButton {
            id: executeButton
            objectName: "executeButton"
            text: "Executar"
            visible: wizardStack.depth === 3
            onClicked: root.viewModel.execute()
        }
    }

    ButtonGroup {
        id: actionGroup
    }

    ButtonGroup {
        id: destinationGroup
    }

    Component {
        id: actionPage

        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.gap

                Label {
                    Layout.fillWidth: true
                    text: "1. Escolha a configuracao inicial dos dados"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.bold: true
                    font.pixelSize: Theme.fontSizeHeader
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: "O assistente criara a arvore canonica e reiniciara o aplicativo com o banco configurado."
                    color: Theme.mutedText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WordWrap
                }

                RadioButton {
                    objectName: "actionCreateEmpty"
                    text: "Criar banco vazio"
                    checked: root.viewModel.action === 0
                    ButtonGroup.group: actionGroup
                    palette.windowText: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    onToggled: {
                        if (checked)
                            root.viewModel.setAction(0);
                    }
                }

                RadioButton {
                    objectName: "actionImportDatabase"
                    text: "Importar um banco SQLite existente"
                    checked: root.viewModel.action === 1
                    ButtonGroup.group: actionGroup
                    palette.windowText: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    onToggled: {
                        if (checked)
                            root.viewModel.setAction(1);
                    }
                }

                RadioButton {
                    objectName: "actionImportXlsx"
                    text: "Criar banco e importar planilhas XLSX"
                    checked: root.viewModel.action === 2
                    ButtonGroup.group: actionGroup
                    palette.windowText: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    onToggled: {
                        if (checked)
                            root.viewModel.setAction(2);
                    }
                }

                RadioButton {
                    objectName: "actionImportDatabaseAndXlsx"
                    text: "Importar banco SQLite e planilhas XLSX"
                    checked: root.viewModel.action === 3
                    ButtonGroup.group: actionGroup
                    palette.windowText: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    onToggled: {
                        if (checked)
                            root.viewModel.setAction(3);
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    Component {
        id: destinationPage

        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.gap

                Label {
                    Layout.fillWidth: true
                    text: "2. Escolha o destino e os arquivos de origem"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.bold: true
                    font.pixelSize: Theme.fontSizeHeader
                    wrapMode: Text.WordWrap
                }

                RadioButton {
                    id: destinationHomeRadio
                    objectName: "destinationHomeRadio"
                    text: "Configurar automaticamente na pasta de usuario"
                    checked: root.viewModel.destinationMode === 0
                    ButtonGroup.group: destinationGroup
                    palette.windowText: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    onToggled: {
                        if (checked)
                            root.viewModel.setDestinationMode(0);
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gap

                    RadioButton {
                        id: destinationCustomRadio
                        objectName: "destinationCustomRadio"
                        text: "Usar outra pasta raiz"
                        checked: root.viewModel.destinationMode === 1
                        ButtonGroup.group: destinationGroup
                        palette.windowText: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        onToggled: {
                            if (checked)
                                root.viewModel.setDestinationMode(1);
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    ActionButton {
                        text: "Escolher pasta..."
                        enabled: destinationCustomRadio.checked
                        implicitWidth: 150
                        onClicked: destinationFolderDialog.open()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Banco de destino: " + root.viewModel.destinationPath
                    color: Theme.mutedText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    elide: Text.ElideMiddle
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderSoft
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.requiresSourceDatabase
                    spacing: Theme.spacingSm

                    Label {
                        text: "Banco SQLite de origem"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.bold: true
                        font.pixelSize: Theme.fontSizeBody
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gap

                        Label {
                            Layout.fillWidth: true
                            text: root.sourceDatabaseSummary
                            color: Theme.mutedText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLabel
                            elide: Text.ElideMiddle
                        }

                        ActionButton {
                            text: "Escolher banco..."
                            implicitWidth: 150
                            onClicked: sourceDbDialog.open()
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.requiresXlsx
                    spacing: Theme.spacingSm

                    Label {
                        text: "Planilhas XLSX de origem"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.bold: true
                        font.pixelSize: Theme.fontSizeBody
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gap

                        Label {
                            Layout.fillWidth: true
                            text: root.xlsxSummary
                            color: Theme.mutedText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLabel
                            elide: Text.ElideMiddle
                        }

                        ActionButton {
                            text: "Escolher XLSX..."
                            implicitWidth: 150
                            onClicked: xlsxDialog.open()
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    Component {
        id: summaryPage

        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.gap

                Label {
                    Layout.fillWidth: true
                    text: "3. Revise e execute"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.bold: true
                    font.pixelSize: Theme.fontSizeHeader
                }

                Label {
                    Layout.fillWidth: true
                    text: "Acao: " + root.actionTitle(root.viewModel.action)
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: "Destino: " + root.viewModel.destinationPath
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WrapAnywhere
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.requiresSourceDatabase
                    text: "Banco de origem: " + root.sourceDatabaseSummary
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WrapAnywhere
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.requiresXlsx
                    text: "Planilhas: " + root.xlsxSummary
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.viewModel.errorMessage.length > 0
                    text: root.viewModel.errorMessage
                    color: Theme.danger
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderSoft
                }

                Label {
                    Layout.fillWidth: true
                    text: "Ao concluir, o aplicativo sera reiniciado com o banco configurado."
                    color: Theme.mutedText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: Text.WordWrap
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    FolderDialog {
        id: destinationFolderDialog
        objectName: "destinationFolderDialog"
        title: "Escolher pasta raiz dos dados"

        onAccepted: root.viewModel.setCustomDestination(selectedFolder)
    }

    FileDialog {
        id: sourceDbDialog
        objectName: "sourceDbDialog"
        title: "Escolher banco SQLite de origem"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Banco SQLite (*.db *.sqlite *.sqlite3)", "Todos os arquivos (*)"]

        onAccepted: {
            root.sourceDatabaseSummary = selectedFile.toString();
            root.viewModel.setSourceDatabase(selectedFile);
        }
    }

    FileDialog {
        id: xlsxDialog
        objectName: "xlsxDialog"
        title: "Escolher planilhas XLSX"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Planilhas XLSX (*.xlsx)"]

        onAccepted: {
            root.xlsxSummary = selectedFiles.length + (selectedFiles.length === 1 ? " planilha selecionada" : " planilhas selecionadas");
            root.viewModel.setXlsxFiles(selectedFiles);
        }
    }

    MessageDialog {
        id: errorDialog
        objectName: "errorDialog"
        title: "Nao foi possivel configurar os dados"
        text: root.viewModel.errorMessage
        buttons: MessageDialog.Ok
    }

    Connections {
        target: root.viewModel

        function onErrorMessageChanged() {
            if (root.viewModel.errorMessage.length > 0)
                errorDialog.open();
        }
    }
}
