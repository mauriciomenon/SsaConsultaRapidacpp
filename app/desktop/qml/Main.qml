pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SsaConsultaRapida

ApplicationWindow {
    id: root
    required property var mainViewModel
    required property var smokeController
    property var vm: mainViewModel

    width: 1420
    height: 860
    minimumWidth: 1100
    minimumHeight: 720
    visible: true
    title: "SSA Consulta Rapida"
    color: Theme.window
    font.family: Theme.fontFamily

    background: Rectangle {
        color: Theme.window
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
        root.vm.browse.load()
    }

    Connections {
        target: root.smokeController

        function onOpenPreferencesRequested() {
            preferencesDialog.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.cardGap
        anchors.rightMargin: Theme.cardGap
        anchors.bottomMargin: Theme.cardGap
        anchors.topMargin: Theme.cardGap
        spacing: Theme.gap

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.panel
            border.color: Theme.border
            radius: Theme.radius

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.cardGap
                spacing: Theme.gap

                Rectangle {
                    Layout.preferredWidth: 4
                    Layout.fillHeight: true
                    color: Theme.accent
                    radius: 99
                }

                Label {
                    Layout.fillWidth: true
                    text: "SSA Consulta Rapida"
                    font.pixelSize: 24
                    color: Theme.text
                    font.bold: true
                    font.letterSpacing: 0.1
                    elide: Text.ElideRight
                }

                Label {
                    text: root.vm.ui.theme === "system" ? "Auto" : root.vm.ui.theme
                    color: Theme.mutedText
                    font.pixelSize: 11
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
                ActionButton {
                    text: "Atualizar"
                    enabled: !root.vm.browse.status.loading
                    onClicked: root.vm.browse.load()
                }
                ActionButton {
                    text: "Exportar"
                    enabled: !root.vm.exports.running
                    onClicked: exportDialog.open()
                }
                ActionButton { text: "Preferencias"; onClicked: preferencesDialog.open() }
                ActionButton {
                    text: "Cancelar consulta"
                    enabled: root.vm.browse.status.loading
                    danger: true
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
                SplitView.minimumWidth: root.vm.ui.detailsVisible ? root.vm.ui.detailsMinimumWidth : 0
                SplitView.preferredWidth: root.vm.ui.detailsVisible ? root.vm.ui.detailsEffectiveWidth : 0
                SplitView.maximumWidth: root.vm.ui.detailsVisible ? root.vm.ui.detailsMaximumWidth : 0
                active: root.vm.ui.detailsVisible
                visible: root.vm.ui.detailsVisible
                focus: false

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
            implicitHeight: 34
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

        function resolvedExportPath() {
            const rawPath = selectedFile.toString().length > 0
                ? selectedFile.toString()
                : fileUrl.toString();
            const resolved = Qt.resolvedUrl(rawPath)
            return resolved.toLocalFile !== undefined && resolved.toLocalFile().length > 0
                   ? resolved.toLocalFile()
                   : resolved.toString()
        }

        onAccepted: {
            const exportPath = resolvedExportPath()
            root.vm.exports.exportFilteredList(exportPath)
        }
    }
}
