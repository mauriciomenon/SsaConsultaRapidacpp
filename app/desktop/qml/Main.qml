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

    palette.toolTipBase: Theme.panelRaised
    palette.toolTipText: Theme.text

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
                text: "Abrir pasta de entrada"
                enabled: !root.vm.actions.commands.running
                onTriggered: root.vm.actions.commands.openInputFolder()
            }
            MenuItem {
                text: "Abrir pasta de processados"
                enabled: !root.vm.actions.commands.running
                onTriggered: root.vm.actions.commands.openProcessedFolder()
            }
            MenuItem {
                text: "Abrir pasta de redundantes"
                enabled: !root.vm.actions.commands.running
                onTriggered: root.vm.actions.commands.openRedundantFolder()
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
            MenuItem {
                text: "Janela de detalhes (grafo)"
                enabled: root.vm.browse.details.selectedSsaNumber.length > 0
                onTriggered: root.openDetailsWindow()
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
            MenuItem {
                text: "Compactar DB"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.compactDatabase()
            }
            MenuSeparator {}
            MenuItem {
                text: "Cancelar consulta"
                enabled: root.vm.browse.status.loading
                onTriggered: root.vm.requestFlow.cancelCurrentRequest()
            }
        }

        Menu {
            title: "Ajuda"

            MenuItem {
                text: "Guia de instalacao"
                enabled: !root.vm.actions.commands.running
                onTriggered: root.vm.actions.commands.openInstallationGuide()
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
            Layout.preferredHeight: Theme.densityValue(root.vm.ui.density, 42, 48, 54)
            color: Theme.header
            border.color: Theme.borderSoft
            radius: Theme.radius

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.densityValue(root.vm.ui.density, 6, 8, 10)
                spacing: Theme.gap

                ActionButton {
                    text: "Abrir SAM"
                    implicitWidth: 116
                    onClicked: root.vm.actions.commands.openSamHome()
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
                    Layout.preferredHeight: Theme.controlHeight
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    leftPadding: 20
                    rightPadding: 20
                    text: root.vm.actions.currentWeek.value + "          " + root.vm.browse.totalRows + " / " + root.vm.browse.totalRowsAll + " SSAs"
                    color: Theme.accent
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    background: Rectangle {
                        color: Theme.window
                        border.color: Theme.border
                        radius: Theme.radius
                    }
                }
                Item {
                    Layout.fillWidth: true
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
            density: root.vm.ui.density
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
            onCopyDerivationSvgRequested: root.vm.copyTextToClipboard(root.vm.browse.details.graphModel.svg)
            onCopyTextRequested: text => root.vm.copyTextToClipboard(text)
            onOpenRequested: root.vm.selectionFlow.openSelectedSsa()
            onConfigureColumnsRequested: columnSelectorPopup.open()
            onNavigateToRelationRequested: ssaNumber => root.vm.selectionFlow.openSsa(ssaNumber)
            onDetailsWindowRequested: root.openDetailsWindow()
        }

        SplitView {
            Layout.fillWidth: true
            Layout.preferredHeight: root.bottomPaneHeight
            Layout.minimumHeight: 280
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 7
                implicitHeight: 7
                color: SplitHandle.pressed ? Theme.accent : SplitHandle.hovered ? Theme.accentSoft : Theme.borderSoft
                border.color: SplitHandle.pressed || SplitHandle.hovered ? Theme.accentStrong : Theme.border
                border.width: 1
            }

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
                    onCopyMermaidRequested: root.vm.copyTextToClipboard(root.vm.browse.details.graphModel.mermaid)
                    onOpenRequested: root.vm.selectionFlow.openSelectedSsa()
                    onGraphWindowRequested: root.openDetailsWindow()
                    onLoadRelationRequested: ssaNumber => root.vm.browse.loadDetailsBySsaNumber(ssaNumber)
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
            weekModel: root.vm.actions.currentWeek
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
        browseViewModel: root.vm.browse
        onDetailsWindowRequested: root.openDetailsWindow()
    }

    FileWorkflowDialogs {
        id: fileDialogs
        viewModel: root.vm
    }

    function openDetailsWindow() {
        const ssaNumber = root.vm.browse.details.selectedSsaNumber;
        if (ssaNumber.length === 0)
            return;
        const window = detailsWindowComponent.createObject(root);
        if (window === null)
            return;
        window.detailsViewModel = root.vm.browse.createDetailsWindowModel(ssaNumber, window);
        window.visible = true;
    }

    Component {
        id: detailsWindowComponent

        SsaDetailsWindow {
            onCopyTextRequested: text => root.vm.copyTextToClipboard(text)
        }
    }
}
