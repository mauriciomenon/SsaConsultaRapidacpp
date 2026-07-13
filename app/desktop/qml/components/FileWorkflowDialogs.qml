pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs

Item {
    id: root
    required property var viewModel

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

    function openDatabase() {
        databaseDialog.open();
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
        title: "Importar XLS/XLSX externo"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Planilhas (*.xls *.xlsx)"]

        onAccepted: root.viewModel.actions.workflows.importExternalFiles(importDataDialog.selectedFiles)
    }
}
