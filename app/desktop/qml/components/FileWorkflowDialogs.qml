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
