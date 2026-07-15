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
    property bool shutdownCloseAccepted: false
    readonly property int bottomPaneHeight: height <= 820 ? 200 : Theme.bottomPaneHeight(height)
    readonly property int paneMinimumHeight: height <= 820 ? 200 : 280

    width: 1580
    height: 940
    minimumWidth: 1180
    minimumHeight: 760
    visible: true
    title: "Consulta Rapida de SSAs"
    color: Theme.window
    font.family: Theme.fontFamily

    onClosing: function (close) {
        if (root.shutdownCloseAccepted || root.vm.shutdownReady)
            return;
        close.accepted = false;
        if (!root.vm.shutdownInProgress)
            root.vm.requestShutdown();
        else if (root.vm.forceCloseAvailable)
            forceShutdownDialog.open();
    }

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
                objectName: "openDatabaseMenuItem"
                text: "Carregar outro banco"
                enabled: !root.vm.databaseSwitch.running
                onTriggered: fileDialogs.openDatabase()
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
                onTriggered: root.close()
            }
        }

        Menu {
            title: "Importacao"

            MenuItem {
                objectName: "openImportDataMenuItem"
                text: "Importar XLSX externo"
                enabled: !root.vm.actions.workflows.running
                onTriggered: fileDialogs.openImportData()
            }
            MenuItem {
                objectName: "openImportDerivationsMenuItem"
                text: "Importar derivadas"
                enabled: !root.vm.actions.workflows.running
                onTriggered: fileDialogs.openImportDerivations()
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
                text: "Limpar referencias orfas"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.cleanOrphanDerivations()
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
            MenuSeparator {}
            MenuItem {
                text: "Atualizar agora"
                enabled: root.vm.actions.workflows.samRefreshEnabled && !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.refreshSamNow()
            }
            MenuItem {
                text: "Atualizacao automatica"
                checkable: true
                checked: root.vm.actions.workflows.samAutoRefreshEnabled
                enabled: root.vm.actions.workflows.samRefreshEnabled
                onTriggered: root.vm.actions.workflows.samAutoRefreshEnabled = checked
            }
            MenuItem {
                text: "Configurar atualizacao"
                onTriggered: root.openSamRefreshDialog()
            }
        }

        Menu {
            title: "Filtros"

            MenuItem {
                text: "Desfazer ultimo filtro"
                enabled: root.vm.browse.canUndoFilters
                onTriggered: root.vm.browse.undoFilters()
            }
            Menu {
                id: undoFilterMenu
                title: "Voltar filtros"
                enabled: root.vm.browse.canUndoFilters

                Instantiator {
                    model: root.vm.browse.filterUndoDepth
                    delegate: MenuItem {
                        required property int index
                        text: index === 0 ? "Voltar 1 nivel" : "Voltar " + (index + 1) + " niveis"
                        onTriggered: root.vm.browse.undoFilterLevels(index + 1)
                    }
                    onObjectAdded: (index, object) => undoFilterMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => undoFilterMenu.removeItem(object)
                }
            }
            MenuItem {
                text: "Copiar historico de filtros"
                enabled: root.vm.browse.canUndoFilters
                onTriggered: root.vm.copyTextToClipboard(root.vm.browse.filterHistoryText())
            }
            MenuSeparator {}
            MenuItem {
                text: "Aplicar filtros"
                onTriggered: root.vm.browse.apply()
            }
            MenuItem {
                text: "Abrir filtros avancados"
                onTriggered: filterPanel.showAdvancedFilters()
            }
            MenuItem {
                text: "Exportar lista"
                onTriggered: fileDialogs.openExportResults()
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
                text: "Ocultar detalhes"
                checkable: true
                checked: root.vm.ui.detailsVisible
                onTriggered: root.vm.ui.detailsVisible = checked
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
                text: "Limpar referencias orfas"
                enabled: !root.vm.actions.workflows.running
                onTriggered: root.vm.actions.workflows.cleanOrphanDerivations()
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
                onTriggered: root.vm.requestCancelAll()
            }
        }

        Menu {
            title: "Ajuda"

            MenuItem {
                text: "Ajuda"
                onTriggered: root.openHelpDialog()
            }
            MenuItem {
                text: "Guia de instalacao"
                enabled: !root.vm.actions.commands.running
                onTriggered: root.vm.actions.commands.openInstallationGuide()
            }
            MenuSeparator {}
            MenuItem {
                text: "Sobre"
                onTriggered: root.openAboutDialog()
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
                    text: "Importar XLSX"
                    enabled: !root.vm.actions.workflows.running
                    implicitWidth: 112
                    onClicked: fileDialogs.openImportData()
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    Layout.preferredHeight: Theme.controlHeight
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    leftPadding: 26
                    rightPadding: 26
                    text: root.vm.actions.currentWeek.value
                    color: Theme.accent
                    font.pixelSize: Theme.fontSizeHeader
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
                Label {
                    Layout.preferredHeight: Theme.controlHeight
                    Layout.rightMargin: 4
                    leftPadding: 16
                    rightPadding: 16
                    text: root.vm.browse.totalRows + " / " + root.vm.browse.totalRowsAll + " SSAs"
                    color: Theme.accent
                    font.pixelSize: Theme.fontSizeBody
                    font.bold: false
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    background: Rectangle {
                        color: Theme.window
                        border.color: Theme.border
                        radius: Theme.radius
                    }
                }
                ActionButton {
                    text: "Preferencias"
                    onClicked: preferencesDialog.open()
                }

                // Theme cycle button: small circle filled with the current
                // theme's accent color. Click cycles to the next theme.
                Button {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    implicitWidth: 28
                    implicitHeight: 28
                    padding: 0
                    topInset: 0
                    bottomInset: 0
                    leftInset: 0
                    rightInset: 0
                    text: ""
                    Accessible.name: "Alternar tema"
                    ToolTip.text: qsTr("Tema")
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    background: Rectangle {
                        radius: 14
                        color: Theme.accent
                        border.color: Theme.border
                        border.width: 1
                    }
                    onClicked: {
                        var order = Theme.themeOptions;
                        var current = Theme.themeName;
                        var idx = order.indexOf(current);
                        var nextIdx = idx < 0 ? 0 : (idx + 1) % order.length;
                        root.vm.ui.theme = order[nextIdx];
                    }
                }
            }
        }

        SearchAndPager {
            Layout.fillWidth: true
            viewModel: root.vm.browse
            preferenceFlow: root.vm.preferenceFlow
            density: root.vm.ui.density
            onExportRequested: fileDialogs.openExportResults()
            onSaveFiltersRequested: root.vm.preferenceFlow.savePreferences()
            onExportFiltersRequested: fileDialogs.openExportFilters()
            onImportFiltersRequested: fileDialogs.openImportFilters()
        }

        SsaTable {
            id: mainTable
            objectName: "mainSsaTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.paneMinimumHeight
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
            id: mainBottomPane
            objectName: "mainBottomPane"
            Layout.fillWidth: true
            Layout.preferredHeight: root.bottomPaneHeight
            Layout.minimumHeight: root.paneMinimumHeight
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
                    density: root.vm.ui.density
                    onGraphWindowRequested: root.openDetailsWindow()
                    onLoadRelationRequested: ssaNumber => root.vm.browse.details.requestLoadBySsaNumber(ssaNumber)
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
            id: mainStatusPill
            objectName: "mainStatusPill"
            Layout.fillWidth: true
            status: root.vm.browse.status
            browse: root.vm.browse
            weekModel: root.vm.actions.currentWeek
            activityController: root.vm
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

    Dialog {
        id: forceShutdownDialog
        objectName: "forceShutdownDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 500
        padding: 20

        background: Rectangle {
            color: Theme.panelRaised
            border.color: Theme.border
            border.width: 1
            radius: Theme.radius
        }

        header: Label {
            text: "Encerramento forcado"
            color: Theme.text
            font.bold: true
            font.pixelSize: Theme.fontSizeTitle
            padding: 20
            bottomPadding: 8
        }

        contentItem: Label {
            text: "Ainda existem operacoes encerrando. Forcar a saida agora?"
            color: Theme.text
            wrapMode: Text.WordWrap
        }

        footer: RowLayout {
            spacing: Theme.spacingSm
            layoutDirection: Qt.RightToLeft

            ActionButton {
                text: "Encerrar"
                danger: true
                onClicked: root.vm.requestForcedShutdown()
            }

            ActionButton {
                text: "Continuar aguardando"
                implicitWidth: 180
                onClicked: forceShutdownDialog.close()
            }
        }
    }

    Connections {
        target: root.vm

        function onShutdownStateChanged() {
            if (!root.vm.shutdownReady)
                return;
            root.shutdownCloseAccepted = true;
            Qt.callLater(root.close);
        }
    }

    Loader {
        id: helpDialogLoader
        active: false
        sourceComponent: HelpDialog {
            visible: true
            onClosing: helpDialogLoader.active = false
        }
    }

    Loader {
        id: aboutDialogLoader
        active: false
        sourceComponent: AboutDialog {
            visible: true
            onClosing: aboutDialogLoader.active = false
        }
    }

    Loader {
        id: samRefreshDialogLoader
        active: false
        sourceComponent: SamRefreshDialog {
            visible: true
            workflowViewModel: root.vm.actions.workflows
            onClosing: samRefreshDialogLoader.active = false
        }
    }

    ColumnSelectorPopup {
        id: columnSelectorPopup
        viewModel: root.vm
    }

    SmokeCaptureBridge {
        smokeController: root.smokeController
        rootContentItem: root.contentItem
        mainTable: mainTable
        bottomPane: mainBottomPane
        statusPill: mainStatusPill
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

    function openHelpDialog() {
        helpDialogLoader.active = true;
    }

    function openAboutDialog() {
        aboutDialogLoader.active = true;
    }

    function openSamRefreshDialog() {
        samRefreshDialogLoader.active = true;
    }

    Component {
        id: detailsWindowComponent

        SsaDetailsWindow {
            onCopyTextRequested: text => root.vm.copyTextToClipboard(text)
        }
    }
}
