pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Dialog {
    id: root
    required property var viewModel

    modal: true
    title: "Preferencias"
    width: Math.min(680, ApplicationWindow.window.width - 80)
    height: Math.min(620, ApplicationWindow.window.height - 80)
    standardButtons: Dialog.NoButton
    onOpened: root.viewModel.discardColumnSettings()
    onClosed: root.viewModel.discardColumnSettings()

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
                Layout.preferredWidth: 160
                model: ["system", "light", "dark"]
                currentIndex: root.viewModel.theme === "dark" ? 2 :
                              root.viewModel.theme === "light" ? 1 : 0
                onActivated: root.viewModel.theme = currentText
            }
            Label {
                Layout.fillWidth: true
                text: "Colunas visiveis e larguras"
                color: Theme.mutedText
                elide: Text.ElideRight
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
                    required property int columnWidth

                    width: columnList.width
                    height: 38
                    color: index % 2 === 0 ? Theme.panel : Theme.rowAlt

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.gap
                        anchors.rightMargin: Theme.gap
                        spacing: Theme.gap

                        CheckBox {
                            checked: columnDelegate.columnVisible
                            onToggled: root.viewModel.columns.setColumnVisible(columnDelegate.index, checked)
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
                            from: 80
                            to: 520
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

            ActionButton { text: "Todas"; onClicked: root.viewModel.columns.selectAll() }
            ActionButton { text: "Padrao"; onClicked: root.viewModel.resetColumnSettings() }
            Item { Layout.fillWidth: true }
            ActionButton {
                text: "Aplicar"
                onClicked: {
                    root.viewModel.applyColumnSettings()
                    root.close()
                }
            }
            ActionButton { text: "Fechar"; onClicked: root.close() }
        }
    }
}
