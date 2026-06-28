pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

ApplicationWindow {
    id: root
    required property var mainViewModel
    property var smokeController: null
    property var vm: mainViewModel
    readonly property int bottomPaneHeight: Theme.bottomPaneHeight(height)

    width: 1580
    height: 940
    minimumWidth: 1180
    minimumHeight: 760
    visible: true
    title: "Consulta Rapida de SSAs"
    color: Theme.window
    font.family: Theme.fontFamily

    background: Rectangle {
        color: Theme.window
    }

    menuBar: MenuBar {
        Menu {
            title: "Arquivo"

            MenuItem {
                text: "Carregar dados"
                enabled: !root.vm.browse.status.loading
                onTriggered: root.vm.browse.load()
            }
            MenuItem {
                text: "Exportar resultados"
                onTriggered: fileDialogs.openExportResults()
            }
            MenuSeparator {}
            MenuItem {
                text: "Sair"
                onTriggered: Qt.quit()
            }
        }

        Menu {
            title: "Importacao"

            MenuItem {
                text: "Importar XLS/XLSX externo"
                enabled: !root.vm.actions.workflows.running
                onTriggered: fileDialogs.openImportData()
            }
            MenuItem {
                text: "Atualizar dados"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.rescanIncremental()
            }
            MenuItem {
                text: "Reescaneamento completo"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.rescanFull()
            }
            MenuSeparator {}
            MenuItem {
                text: "Atualizar derivadas"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.syncDerivadas()
            }
        }

        Menu {
            title: "SAM"

            MenuItem {
                text: "Abrir SAM"
                onTriggered: root.vm.actions.commands.openSamHome()
            }
            MenuItem {
                text: "Abrir SSA selecionada"
                enabled: root.vm.browse.details.selectedSsaNumber.length > 0
                onTriggered: root.vm.selectionFlow.openSelectedSsa()
            }
        }

        Menu {
            title: "Filtros"

            MenuItem {
                text: "Aplicar filtros"
                onTriggered: root.vm.browse.apply()
            }
            MenuItem {
                text: "Abrir filtros avancados"
                onTriggered: filterPanel.showAdvancedFilters()
            }
            MenuItem {
                text: "Exportar filtros"
                onTriggered: fileDialogs.openExportFilters()
            }
            MenuItem {
                text: "Importar filtros"
                onTriggered: fileDialogs.openImportFilters()
            }
            MenuSeparator {}
            MenuItem {
                text: "Salvar preferencias"
                onTriggered: root.vm.preferenceFlow.savePreferences()
            }
        }

        Menu {
            title: "Exibir"

            MenuItem {
                text: "Preferencias"
                onTriggered: preferencesDialog.open()
            }
            MenuItem {
                text: root.vm.ui.detailsVisible ? "Ocultar detalhes" : "Mostrar detalhes"
                onTriggered: root.vm.ui.detailsVisible = !root.vm.ui.detailsVisible
            }
        }

        Menu {
            title: "Manutencao"

            MenuItem {
                text: "Atualizar dados"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.rescanIncremental()
            }
            MenuItem {
                text: "Reescaneamento completo"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.rescanFull()
            }
            MenuSeparator {}
            MenuItem {
                text: "Atualizar derivadas"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.syncDerivadas()
            }
            MenuSeparator {}
            MenuItem {
                text: "Cancelar consulta"
                enabled: root.vm.browse.status.loading
                onTriggered: root.vm.requestFlow.cancelCurrentRequest()
            }
        }
    }

    Binding {
        target: Theme
        property: "themeName"
        value: root.vm.ui.resolvedTheme
    }

    Binding {
        target: root.vm.ui
        property: "detailsViewportWidth"
        value: root.width
    }

    Component.onCompleted: {
        root.vm.browse.load();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: Theme.gap

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.header
            border.color: Theme.borderSoft
            radius: Theme.radius

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: Theme.gap

                ActionButton {
                    text: "Abrir SAM"
                    implicitWidth: 116
                    onClicked: root.vm.actions.commands.openSamHome()
                }
                ActionButton {
                    text: "Carregar Dados"
                    enabled: !root.vm.browse.status.loading
                    implicitWidth: 132
                    onClicked: root.vm.browse.load()
                }
                ActionButton {
                    text: "Importar XLS"
                    enabled: !root.vm.actions.workflows.running
                    implicitWidth: 112
                    onClicked: fileDialogs.openImportData()
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: Theme.controlHeight
                    text: root.vm.actions.currentWeek.value
                    color: Theme.accent
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    background: Rectangle {
                        color: Theme.panelRaised
                        border.color: Theme.border
                        radius: Theme.radius
                    }
                }
                Label {
                    Layout.preferredWidth: 250
                    Layout.preferredHeight: Theme.controlHeight
                    text: root.vm.browse.totalRows + " / " + root.vm.browse.totalRowsAll + " SSAs"
                    color: Theme.accent
                    font.pixelSize: 14
                    font.bold: false
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    background: Rectangle {
                        color: Theme.surface
                        border.color: Theme.borderSoft
                        radius: Theme.radius
                    }
                }
                Label {
                    Layout.preferredWidth: 360
                    Layout.preferredHeight: Theme.controlHeight
                    text: root.vm.browse.status.error.length > 0 ? root.vm.browse.status.error : root.vm.browse.status.message
                    color: root.vm.browse.status.error.length > 0 ? Theme.dangerStrong : Theme.accent
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                    elide: Text.ElideRight
                    background: Rectangle {
                        color: Theme.panelRaised
                        border.color: root.vm.browse.status.error.length > 0 ? Theme.danger : Theme.border
                        radius: Theme.radius
                    }
                }
                ActionButton {
                    text: "Preferencias"
                    onClicked: preferencesDialog.open()
                }
                ActionButton {
                    text: "Cancelar"
                    enabled: root.vm.browse.status.loading
                    danger: true
                    onClicked: root.vm.requestFlow.cancelCurrentRequest()
                }
            }
        }

        SearchAndPager {
            Layout.fillWidth: true
            viewModel: root.vm.browse
            onExportRequested: fileDialogs.openExportResults()
            onSaveFiltersRequested: root.vm.preferenceFlow.savePreferences()
            onExportFiltersRequested: fileDialogs.openExportFilters()
            onImportFiltersRequested: fileDialogs.openImportFilters()
        }

        SsaTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 280
            viewModel: root.vm.browse
            columnSettings: root.vm.columns
            columnFlow: root.vm.columnFlow
            density: root.vm.ui.density
            onOpenRequested: root.vm.selectionFlow.openSelectedSsa()
            onConfigureColumnsRequested: columnSelectorPopup.open()
        }

        SplitView {
            Layout.fillWidth: true
            Layout.preferredHeight: root.bottomPaneHeight
            Layout.minimumHeight: 280
            orientation: Qt.Horizontal

            Loader {
                SplitView.minimumWidth: root.vm.ui.detailsVisible ? root.vm.ui.detailsMinimumWidth : 0
                SplitView.preferredWidth: root.vm.ui.detailsVisible ? root.vm.ui.detailsPreferredWidth : 0
                SplitView.maximumWidth: root.vm.ui.detailsVisible ? root.vm.ui.detailsMaximumWidth : 0
                active: root.vm.ui.detailsVisible
                visible: root.vm.ui.detailsVisible
                focus: false

                sourceComponent: DetailsPanel {
                    viewModel: root.vm.browse.details
                    browseViewModel: root.vm.browse
                    density: root.vm.ui.density
                    onOpenRequested: root.vm.selectionFlow.openSelectedSsa()
                }
            }

            FilterPanel {
                id: filterPanel
                SplitView.minimumWidth: 520
                SplitView.fillWidth: true
                filterViewModel: root.vm.browse.filters
                onApplyRequested: root.vm.browse.apply()
            }
        }

        StatusPill {
            Layout.fillWidth: true
            status: root.vm.browse.status
            browse: root.vm.browse
            implicitHeight: 28
        }
    }

    PreferencesDialog {
        id: preferencesDialog
        viewModel: root.vm
        themeDialog: themeDialog
    }

    ThemeDialog {
        id: themeDialog
        viewModel: root.vm
    }

    ColumnSelectorPopup {
        id: columnSelectorPopup
        viewModel: root.vm
    }

    SmokeCaptureBridge {
        smokeController: root.smokeController
        preferencesDialog: preferencesDialog
        filterPanel: filterPanel
    }

    FileWorkflowDialogs {
        id: fileDialogs
        viewModel: root.vm
    }
}
