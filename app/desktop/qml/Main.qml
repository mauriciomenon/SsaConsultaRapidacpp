pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SsaConsultaRapida

ApplicationWindow {
    id: root
    required property var mainViewModel
    property var vm: mainViewModel

    width: 1420
    height: 860
    minimumWidth: 1100
    minimumHeight: 720
    visible: true
    title: "SSA Consulta Rapida"
    color: Theme.window
    font.family: Theme.fontFamily

    Binding {
        target: Theme
        property: "themeName"
        value: root.vm.ui.theme === "system"
               ? (Application.styleHints.colorScheme === Qt.ColorScheme.Dark ? "dark" : "light")
               : root.vm.ui.theme
    }

    Component.onCompleted: root.vm.browse.load()

    function openPreferencesForSmoke() {
        preferencesDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: Theme.gap

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: Theme.panel
            border.color: Theme.border
            radius: Theme.radius

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.gap
                spacing: Theme.gap

                Label {
                    Layout.fillWidth: true
                    text: "SSA Consulta Rapida"
                    font.pixelSize: 18
                    font.bold: true
                    color: Theme.text
                    elide: Text.ElideRight
                }
                ActionButton { text: "SAM"; onClicked: root.vm.commands.openSamHome() }
                ActionButton {
                    id: openButton
                    text: "Abrir"
                    onClicked: openMenu.open()

                    Menu {
                        id: openMenu
                        y: openButton.height

                        MenuItem {
                            text: "Pasta entrada"
                            onTriggered: root.vm.commands.openInputFolder()
                        }
                        MenuItem {
                            text: "Processados"
                            onTriggered: root.vm.commands.openProcessedFolder()
                        }
                        MenuItem {
                            text: "Redundantes"
                            onTriggered: root.vm.commands.openRedundantFolder()
                        }
                        MenuItem {
                            text: "Guia instalacao"
                            onTriggered: root.vm.commands.openInstallationGuide()
                        }
                    }
                }
                ActionButton { text: "Atualizar"; onClicked: root.vm.browse.load() }
                ActionButton {
                    text: "Exportar"
                    enabled: !root.vm.exports.running
                    onClicked: exportDialog.open()
                }
                ActionButton { text: "Preferencias"; onClicked: preferencesDialog.open() }
                ActionButton {
                    text: "Cancelar consulta"
                    enabled: root.vm.browse.status.loading
                    onClicked: root.vm.browse.cancelCurrentRequest()
                }
            }
        }

        SearchAndPager {
            Layout.fillWidth: true
            viewModel: root.vm.browse
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            ColumnLayout {
                SplitView.minimumWidth: 720
                SplitView.fillWidth: true
                spacing: Theme.gap

                FilterPanel {
                    Layout.fillWidth: true
                    filterViewModel: root.vm.browse.filters
                    onApplyRequested: root.vm.browse.apply()
                }

                SsaTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    viewModel: root.vm.browse
                    density: root.vm.ui.density
                    onOpenRequested: root.vm.openSelectedSsa()
                }
            }

            Loader {
                SplitView.minimumWidth: root.vm.ui.detailsVisible ? 280 : 0
                SplitView.preferredWidth: root.vm.ui.detailsVisible ? root.vm.ui.detailsPanelWidth : 0
                SplitView.maximumWidth: root.vm.ui.detailsVisible ? 900 : 0
                active: root.vm.ui.detailsVisible
                visible: root.vm.ui.detailsVisible

                sourceComponent: DetailsPanel {
                    viewModel: root.vm.browse.details
                    density: root.vm.ui.density
                    onOpenRequested: root.vm.openSelectedSsa()
                }
            }
        }

        StatusPill {
            Layout.fillWidth: true
            status: root.vm.browse.status
            browse: root.vm.browse
        }
    }

    PreferencesDialog {
        id: preferencesDialog
        viewModel: root.vm
    }

    FileDialog {
        id: exportDialog
        title: "Exportar CSV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV (*.csv)"]
        onAccepted: root.vm.exports.exportFilteredList(selectedFile)
    }
}
