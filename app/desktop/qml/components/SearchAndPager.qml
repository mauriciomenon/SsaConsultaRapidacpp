pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
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

    Component.onCompleted: Qt.callLater(root.focusSearchInput)
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(root.focusSearchInput);
        }
    }

    Layout.preferredHeight: Theme.densityValue(root.density, 108, 122, 132)
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
                text: "Limpar"
                implicitWidth: 88
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: {
                    root.viewModel.search.clear();
                    root.viewModel.search.apply();
                }
            }
            AppTextField {
                id: searchInput
                Layout.fillWidth: true
                text: root.viewModel.search.text
                placeholderText: "Busca geral"
                placeholderTextColor: Theme.mutedText
                font.pixelSize: 12
                onTextEdited: root.viewModel.search.text = text
                onAccepted: {
                    root.viewModel.search.apply();
                }
            }
            ActionButton {
                text: "Aplicar"
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: root.viewModel.search.apply()
            }
            ActionButton {
                text: "Exportar"
                implicitWidth: 124
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: root.exportRequested()
            }
            ActionButton {
                id: filterMenuButton
                text: "Filtros"
                implicitWidth: 96
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: filterMenu.open()

                Menu {
                    id: filterMenu
                    y: filterMenuButton.height

                    MenuItem {
                        text: "Salvar Consulta"
                        onTriggered: root.saveFiltersRequested()
                    }
                    MenuItem {
                        text: "Exportar Filtros"
                        onTriggered: root.exportFiltersRequested()
                    }
                    MenuItem {
                        text: "Importar Filtros"
                        onTriggered: root.importFiltersRequested()
                    }
                }
            }
        }

        FilterSummaryBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.densityValue(root.density, 30, 34, 38)
            filterViewModel: root.filterViewModel
            searchText: root.viewModel.search.text
            onClearSearchRequested: {
                root.viewModel.search.clear();
                root.viewModel.search.apply();
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ActionButton {
                text: "<"
                implicitWidth: 40
                implicitHeight: Theme.controlHeight - 4
                enabled: root.viewModel.pageNumber > 1
                onClicked: root.viewModel.previousPage()
            }
            ActionButton {
                text: ">"
                implicitWidth: 40
                implicitHeight: Theme.controlHeight - 4
                enabled: root.viewModel.pageNumber < root.viewModel.pageCount
                onClicked: root.viewModel.nextPage()
            }
            AppSpinBox {
                id: pageSizeSpin
                from: 10
                to: 500
                stepSize: 5
                value: root.viewModel.pageSize
                Layout.preferredWidth: 72
                onValueModified: {
                    root.viewModel.pageSize = pageSizeSpin.value;
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: "Setor:"
                color: Theme.accent
                font.pixelSize: 12
                font.bold: false
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
            AppComboBox {
                id: sectorFilter

                Layout.preferredWidth: 180
                model: root.filterViewModel.sector.selectorValues
                currentIndex: root.filterViewModel.sector.selectorIndex
                implicitHeight: Theme.controlHeight - 4
                font.pixelSize: 11
                font.bold: false
                onActivated: {
                    root.filterViewModel.sector.quickSector = sectorFilter.currentText;
                    root.viewModel.apply();
                }
                delegate: ItemDelegate {
                    required property string modelData
                    width: sectorFilter.width
                    text: modelData.length === 0 ? "Todos" : modelData
                }
            }
        }
    }
}
