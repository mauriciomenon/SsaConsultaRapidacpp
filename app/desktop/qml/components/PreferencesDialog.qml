pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SsaConsultaRapida

Window {
    id: root
    objectName: DesktopSmokeObjectNames.preferencesDialog

    required property var viewModel
    property var themeDialog: null
    readonly property int detailsCurrentWidth: root.viewModel.ui.detailsPanelWidth
    readonly property int detailsWidthMin: root.viewModel.ui.detailsMinimumWidth
    readonly property int detailsWidthMax: root.viewModel.ui.detailsMaximumWidth
    readonly property int availableScreenWidth: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 1280
    readonly property int availableScreenHeight: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 840
    readonly property int availableScreenX: Screen.virtualX
    readonly property int availableScreenY: Screen.virtualY
    function centeredCoordinate(availableOrigin, availableSize, windowSize) {
        return availableOrigin + Math.min(Math.max(0, Math.round((availableSize - windowSize) / 2)), Math.max(0, availableSize - windowSize));
    }

    title: "Preferencias"
    modality: Qt.ApplicationModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
    color: Theme.window
    minimumWidth: Math.min(900, availableScreenWidth)
    minimumHeight: Math.min(620, availableScreenHeight)
    width: Math.min(availableScreenWidth, Math.max(minimumWidth, Math.min(1120, availableScreenWidth - 160)))
    height: Math.min(availableScreenHeight, Math.max(minimumHeight, Math.min(760, availableScreenHeight - 120)))
    x: centeredCoordinate(availableScreenX, availableScreenWidth, width)
    y: centeredCoordinate(availableScreenY, availableScreenHeight, height)

    function open() {
        root.show();
        root.raise();
        root.requestActivate();
    }

    function applyColumnChanges() {
        root.viewModel.columnFlow.applyColumnSettings();
    }

    function discardColumnChanges() {
        root.viewModel.columnFlow.discardColumnSettings();
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: Theme.cardGap

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 142
                color: Theme.surface
                border.color: Theme.border
                radius: Theme.radius

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    columns: 6
                    columnSpacing: Theme.gap
                    rowSpacing: Theme.gap

                    Label {
                        text: "Tema"
                        color: Theme.text
                        font.bold: true
                    }
                    AppComboBox {
                        Layout.preferredWidth: 150
                        model: Theme.themeOptions
                        currentIndex: Theme.themeOptions.indexOf(root.viewModel.ui.theme) >= 0 ? Theme.themeOptions.indexOf(root.viewModel.ui.theme) : Theme.themeOptions.indexOf("system")
                        onActivated: function (index) {
                            root.viewModel.ui.theme = Theme.themeOptions[index];
                        }
                    }

                    Label {
                        text: "Densidade"
                        color: Theme.text
                        font.bold: true
                    }
                    AppComboBox {
                        readonly property var densityOptions: ["compact", "normal", "comfortable"]
                        readonly property int densityIndex: densityOptions.indexOf(root.viewModel.ui.density)

                        Layout.preferredWidth: 160
                        model: densityOptions
                        currentIndex: densityIndex >= 0 ? densityIndex : densityOptions.indexOf("normal")
                        onActivated: function (index) {
                            root.viewModel.ui.density = densityOptions[index];
                        }
                    }

                    AppCheckBox {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        text: "Mostrar detalhes"
                        checked: root.viewModel.ui.detailsVisible
                        onToggled: root.viewModel.ui.detailsVisible = checked
                    }

                    Label {
                        text: "Largura detalhes"
                        color: Theme.text
                        font.bold: true
                    }
                    AppSpinBox {
                        id: detailsWidthInput
                        from: root.detailsWidthMin
                        to: root.detailsWidthMax
                        value: root.detailsCurrentWidth
                        stepSize: 5
                        Layout.preferredWidth: 120
                        onValueModified: root.viewModel.ui.detailsPanelWidth = value
                    }
                    ActionButton {
                        text: "Personalizar tema..."
                        implicitWidth: 150
                        enabled: root.themeDialog !== null
                        onClicked: root.themeDialog.open()
                    }

                    Label {
                        text: "Linhas por lote"
                        color: Theme.text
                        font.bold: true
                    }
                    AppSpinBox {
                        from: 1
                        to: 1000
                        value: root.viewModel.actions.workflows.importRowsPerChunk
                        stepSize: 1
                        Layout.preferredWidth: 120
                        onValueModified: root.viewModel.actions.workflows.importRowsPerChunk = value
                    }

                    Label {
                        text: "Espera SQLite (ms)"
                        color: Theme.text
                        font.bold: true
                    }
                    AppSpinBox {
                        from: 0
                        to: 3000
                        value: root.viewModel.actions.workflows.importSqliteBusyWaitMs
                        stepSize: 5
                        Layout.preferredWidth: 120
                        onValueModified: root.viewModel.actions.workflows.importSqliteBusyWaitMs = value
                    }

                    Label {
                        Layout.columnSpan: 3
                        Layout.fillWidth: true
                        text: "Colunas visiveis e largura da tabela"
                        color: Theme.mutedText
                        elide: Text.ElideRight
                    }
                }
            }

            ColumnConfigList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                viewModel: root.viewModel.columns
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: Theme.surface
                border.color: Theme.border
                radius: Theme.radius

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: Theme.gap

                    ActionButton {
                        text: "Selecionar tudo"
                        implicitWidth: 132
                        onClicked: root.viewModel.columns.selectAll()
                    }
                    ActionButton {
                        text: "Restaurar padrao"
                        implicitWidth: 140
                        onClicked: root.viewModel.columnFlow.resetColumnSettings()
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    ActionButton {
                        text: "Aplicar colunas"
                        implicitWidth: 140
                        onClicked: root.applyColumnChanges()
                    }
                    ActionButton {
                        text: "Reverter colunas"
                        implicitWidth: 148
                        onClicked: root.discardColumnChanges()
                    }
                    ActionButton {
                        text: "Fechar"
                        implicitWidth: 104
                        onClicked: root.close()
                    }
                }
            }
        }
    }
}
