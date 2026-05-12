pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Dialog {
    id: root
    required property var viewModel
    readonly property int hostAvailableWidth: ApplicationWindow.window ? ApplicationWindow.window.width : 1420
    readonly property int hostAvailableHeight: ApplicationWindow.window ? ApplicationWindow.window.height : 860

    modal: true
    title: "Preferencias"
    width: Math.max(560, Math.min(760, hostAvailableWidth - 80))
    height: Math.max(460, Math.min(660, hostAvailableHeight - 80))
    x: Math.round((hostAvailableWidth - width) / 2)
    y: Math.round((hostAvailableHeight - height) / 2)
    closePolicy: Popup.NoAutoClose
    standardButtons: Dialog.NoButton

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.border
        radius: Theme.radius
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                text: "Tema"
                color: Theme.text
                Layout.preferredWidth: 90
            }
            ComboBox {
                readonly property var themeOptions: ["system", "light", "dark"]

                Layout.preferredWidth: 160
                model: themeOptions
                currentIndex: Math.max(0, themeOptions.indexOf(root.viewModel.theme))
                onActivated: root.viewModel.theme = currentText
            }
            Label {
                text: "Densidade"
                color: Theme.text
                Layout.preferredWidth: 82
            }
            ComboBox {
                readonly property var densityOptions: ["compact", "normal", "comfortable"]

                Layout.preferredWidth: 150
                model: densityOptions
                currentIndex: Math.max(0, densityOptions.indexOf(root.viewModel.density))
                onActivated: root.viewModel.density = currentText
            }
            Label {
                Layout.fillWidth: true
                text: "Aparencia e exibicao"
                color: Theme.mutedText
                elide: Text.ElideRight
            }
        }

        AppCheckBox {
            Layout.fillWidth: true
            text: "Mostrar painel de detalhes"
            checked: root.viewModel.detailsVisible
            onToggled: root.viewModel.detailsVisible = checked
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                text: "Largura detalhes"
                color: Theme.text
                Layout.preferredWidth: 160
            }
            SpinBox {
                from: 280
                to: 680
                stepSize: 20
                value: root.viewModel.detailsPanelWidth
                onValueModified: root.viewModel.detailsPanelWidth = value
            }
            Label {
                Layout.fillWidth: true
                text: "pixels"
                color: Theme.mutedText
                elide: Text.ElideRight
            }
        }

        TextField {
            id: columnSearch
            Layout.fillWidth: true
            placeholderText: "Filtrar colunas por nome ou chave"
            selectByMouse: true
            Timer {
                id: columnFilterTimer
                interval: 120
                repeat: false
                onTriggered: root.viewModel.columns.setFilterText(columnSearch.text)
            }
            onTextChanged: {
                columnFilterTimer.stop()
                columnFilterTimer.start()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.window
            border.color: Theme.border
            radius: Theme.radius
            clip: true

            ListView {
            id: columnList
            anchors.fill: parent
            anchors.margins: 1
            model: root.viewModel.columns
            clip: true

                delegate: Rectangle {
                    id: columnDelegate
                    required property int index
                    required property string columnKey
                    required property string columnLabel
                    required property bool columnVisible
                    required property bool columnToggleEnabled
                    required property int columnWidth

                    width: columnList.width
                    height: 38
                    color: Theme.panel

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.gap
                        anchors.rightMargin: Theme.gap
                        spacing: Theme.gap

                        CheckBox {
                            checked: columnDelegate.columnVisible
                            enabled: columnDelegate.columnToggleEnabled
                            onToggled: root.viewModel.columns.setColumnVisibleByKey(
                                columnDelegate.columnKey, checked)
                        }
                        Label {
                            Layout.preferredWidth: 210
                            text: columnDelegate.columnLabel
                            color: Theme.text
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: columnDelegate.columnKey
                            color: Theme.mutedText
                            elide: Text.ElideRight
                        }
                        SpinBox {
                            id: widthSpin
                            from: root.viewModel.columns.minColumnWidth
                            to: root.viewModel.columns.maxColumnWidth
                            stepSize: 10
                            value: columnDelegate.columnWidth
                            onValueModified: root.viewModel.columns.setColumnWidth(columnDelegate.columnKey, value)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ActionButton { text: "Selecionar tudo"; onClicked: root.viewModel.columns.selectAll() }
            ActionButton { text: "Restaurar colunas"; onClicked: root.viewModel.resetColumnSettings() }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: "Salvar colunas"
                onClicked: {
                    root.viewModel.applyColumnSettings()
                    root.close()
                }
            }
            ActionButton {
                text: "Descartar colunas"
                onClicked: {
                    root.viewModel.discardColumnSettings()
                    root.close()
                }
            }
        }
    }
}
