pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs

Item {
    id: root
    objectName: "fileWorkflowDialogs"
    required property var viewModel
    property var pendingDerivadasFiles: []

    function openExportResults() {
        exportDialog.open();
    }

    function openExportFilters() {
        exportFiltersDialog.open();
    }

    function openImportFilters() {
        importFiltersDialog.open();
    }

    function openImportData() {
        importDataDialog.open();
    }

    function openImportDerivations() {
        derivadasDialog.open();
    }

    function openDatabase() {
        databaseDialog.open();
    }

    function requestDerivadasImport(files) {
        const requiresLegacyConverter = files.some(file => file.toString().toLowerCase().endsWith(".xls"));
        if (requiresLegacyConverter) {
            if (!root.viewModel.actions.workflows.legacyDerivadasConverterAvailable()) {
                root.pendingDerivadasFiles = [];
                legacyDerivadasUnavailableDialog.open();
                return;
            }
            root.pendingDerivadasFiles = files;
            legacyDerivadasPreflightDialog.open();
            return;
        }
        root.viewModel.actions.workflows.importDerivations(files);
    }

    Connections {
        target: root.viewModel.databaseSwitch

        function onErrorMessageChanged() {
            if (root.viewModel.databaseSwitch.errorMessage.length > 0)
                databaseErrorDialog.open();
        }
    }

    MessageDialog {
        id: databaseErrorDialog
        objectName: "databaseErrorDialog"
        title: "Nao foi possivel carregar o banco"
        text: root.viewModel.databaseSwitch.errorMessage
    }

    FileDialog {
        id: databaseDialog
        objectName: "databaseFileDialog"
        title: "Carregar outro banco"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Banco SQLite (*.db *.sqlite *.sqlite3)", "Todos os arquivos (*)"]

        onAccepted: root.viewModel.databaseSwitch.openDatabase(databaseDialog.selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Exportar CSV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV (*.csv)"]

        onAccepted: root.viewModel.actions.exports.exportFilteredList(exportDialog.selectedFile)
    }

    FileDialog {
        id: exportFiltersDialog
        title: "Exportar filtros"
        fileMode: FileDialog.SaveFile
        nameFilters: ["JSON (*.json)"]

        onAccepted: root.viewModel.preferenceFlow.exportFilterPreset(exportFiltersDialog.selectedFile)
    }

    FileDialog {
        id: importFiltersDialog
        title: "Importar filtros"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON (*.json)"]

        onAccepted: root.viewModel.preferenceFlow.importFilterPreset(importFiltersDialog.selectedFile)
    }

    FileDialog {
        id: importDataDialog
        objectName: "importDataFileDialog"
        title: "Importar XLSX externo"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Planilhas XLSX (*.xlsx)"]

        onAccepted: root.viewModel.actions.workflows.importExternalFiles(importDataDialog.selectedFiles)
    }

    FileDialog {
        id: derivadasDialog
        objectName: "derivadasFileDialog"
        title: "Importar derivadas"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Derivadas (*.csv *.txt *.tsv *.xlsx *.xlsm)", "XLS legado - requer LibreOffice (*.xls)"]

        onAccepted: root.requestDerivadasImport(derivadasDialog.selectedFiles)
    }

    MessageDialog {
        id: legacyDerivadasPreflightDialog
        objectName: "legacyDerivadasPreflightDialog"
        title: "Importar XLS legado"
        text: "A importacao XLS legado requer LibreOffice instalado. Confirmar a conversao explicita?"
        buttons: MessageDialog.Ok | MessageDialog.Cancel

        onAccepted: {
            root.viewModel.actions.workflows.importDerivations(root.pendingDerivadasFiles);
            root.pendingDerivadasFiles = [];
        }
        onRejected: root.pendingDerivadasFiles = []
    }

    MessageDialog {
        id: legacyDerivadasUnavailableDialog
        objectName: "legacyDerivadasUnavailableDialog"
        title: "LibreOffice nao encontrado"
        text: "A importacao XLS legado nao pode iniciar porque o executavel LibreOffice soffice nao foi encontrado."
        buttons: MessageDialog.Ok
    }
}
