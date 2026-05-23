pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    readonly property var filterViewModel: viewModel.filters
    signal exportRequested()
    signal saveFiltersRequested()
    signal exportFiltersRequested()
    signal importFiltersRequested()
    signal configureColumnsRequested()
    focus: true

    function focusSearchInput() {
        if (root.visible) {
            searchInput.forceActiveFocus()
        }
    }

    Component.onCompleted: Qt.callLater(root.focusSearchInput)
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(root.focusSearchInput)
        }
    }

    Layout.preferredHeight: 136
    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ActionButton {
                text: "Limpar"
                implicitWidth: 88
                onClicked: {
                    root.viewModel.search.clear()
                    root.viewModel.search.apply()
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
                    root.viewModel.search.apply()
                }
            }
            ActionButton {
                text: "Aplicar"
                onClicked: root.viewModel.search.apply()
            }
            ActionButton {
                text: "Exportar"
                implicitWidth: 124
                onClicked: root.exportRequested()
            }
            ActionButton {
                id: filterMenuButton
                text: "Filtros"
                implicitWidth: 96
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.panelRaised
            border.color: Theme.border
            radius: Theme.radius

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: Theme.gap

                Label {
                    text: root.filterViewModel.activeFilterSummary.length > 0
                          ? root.filterViewModel.activeFilterSummary
                          : "Sem filtros manuais"
                    color: root.filterViewModel.activeFilterSummary.length > 0
                           ? Theme.text
                           : Theme.mutedText
                    font.bold: root.filterViewModel.activeFilterSummary.length > 0
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ActionButton {
                text: "Pagina Anterior"
                implicitWidth: 130
                enabled: root.viewModel.pageNumber > 1
                onClicked: root.viewModel.previousPage()
            }
            Label {
                Layout.preferredWidth: 150
                font.pixelSize: 12
                text: root.viewModel.pageSummary
                color: Theme.accent
                font.bold: true
                elide: Text.ElideRight
            }
            ActionButton {
                text: "Proxima Pagina"
                implicitWidth: 130
                enabled: root.viewModel.pageNumber < root.viewModel.pageCount
                onClicked: root.viewModel.nextPage()
            }
            Label {
                text: "Linhas:"
                color: Theme.accent
                font.pixelSize: 12
                font.bold: true
            }
            AppSpinBox {
                id: pageSizeSpin
                from: 10
                to: 500
                stepSize: 5
                value: root.viewModel.pageSize
                Layout.preferredWidth: 92
                onValueModified: {
                    root.viewModel.pageSize = pageSizeSpin.value
                }
            }
            ActionButton {
                Layout.preferredWidth: 360
                Layout.fillWidth: true
                text: "Colunas: " + root.viewModel.tableHeaders.length
                onClicked: root.configureColumnsRequested()
            }
            Label {
                text: "Setor:"
                color: Theme.accent
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
            AppComboBox {
                id: sectorFilter

                Layout.preferredWidth: 220
                model: root.filterViewModel.sector.selectorValues
                currentIndex: root.filterViewModel.sector.selectorIndex
                font.pixelSize: 12
                font.bold: true
                onActivated: {
                    root.filterViewModel.sector.quickSector = sectorFilter.currentText
                    root.viewModel.apply()
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
