pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    required property var preferenceFlow
    property string density: "normal"
    readonly property var filterViewModel: viewModel.filters
    signal exportRequested
    signal saveFiltersRequested
    signal exportFiltersRequested
    signal importFiltersRequested
    focus: true

    function focusSearchInput() {
        if (root.visible) {
            searchInput.forceActiveFocus();
        }
    }

    function openSaveFilterDialog() {
        savedFilterControls.openSaveFilterDialog();
    }

    Component.onCompleted: Qt.callLater(root.focusSearchInput)
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(root.focusSearchInput);
        }
    }

    Layout.preferredHeight: Theme.densityValue(root.density, 126, 142, 158)
    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.densityValue(root.density, 6, 8, 10)
        spacing: Theme.densityValue(root.density, 5, Theme.gap, 10)

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ActionButton {
                text: "Desfazer"
                enabled: root.viewModel.canUndoFilters
                implicitWidth: 88
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: root.viewModel.undoFilters()
            }
            ActionButton {
                text: "Limpar"
                implicitWidth: 88
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: root.viewModel.search.clear()
            }
            AppTextField {
                id: searchInput
                Layout.fillWidth: true
                text: root.viewModel.search.text
                placeholderText: "Busca geral"
                placeholderTextColor: Theme.mutedText
                font.pixelSize: Theme.fontSizeBody
                onTextEdited: root.viewModel.search.text = text
                onAccepted: {
                    root.viewModel.search.apply();
                }
            }
            ActionButton {
                text: "Aplicar"
                implicitWidth: 82
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: root.viewModel.search.apply()
            }
            SavedFilterControls {
                id: savedFilterControls
                viewModel: root.viewModel
                filterViewModel: root.filterViewModel
                preferenceFlow: root.preferenceFlow
                density: root.density
                onExportRequested: root.exportRequested()
                onSaveFiltersRequested: root.saveFiltersRequested()
                onExportFiltersRequested: root.exportFiltersRequested()
                onImportFiltersRequested: root.importFiltersRequested()
            }
        }

        FilterSummaryBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.densityValue(root.density, 30, 34, 38)
            filterViewModel: root.filterViewModel
            searchText: root.viewModel.search.text
            onClearSearchRequested: {
                root.viewModel.search.clear();
            }
            onClearAllRequested: {
                root.viewModel.search.text = "";
                root.filterViewModel.resetFilters();
            }
        }

        PagerQuickFilters {
            Layout.fillWidth: true
            viewModel: root.viewModel
            filterViewModel: root.filterViewModel
        }
    }
}
