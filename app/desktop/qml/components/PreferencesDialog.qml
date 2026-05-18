pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Dialog {
    id: root
    required property var viewModel
    readonly property int hostAvailableWidth: parent && parent.width > 0 ? parent.width : 1280
    readonly property int hostAvailableHeight: parent && parent.height > 0 ? parent.height : 840
    readonly property int detailsCurrentWidth: root.viewModel.ui.detailsEffectiveWidth
    readonly property int detailsWidthMin: root.viewModel.ui.detailsMinimumWidth
    readonly property int detailsWidthMax: root.viewModel.ui.detailsMaximumWidth

    modal: true
    title: "Preferencias"
    width: Math.max(760, Math.min(1080, hostAvailableWidth - 140))
    height: Math.max(560, Math.min(740, hostAvailableHeight - 120))
    x: Math.round((hostAvailableWidth - width) / 2)
    y: Math.round((hostAvailableHeight - height) / 2)
    padding: 0
    closePolicy: Popup.NoAutoClose
    standardButtons: Dialog.NoButton

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.border
        radius: Theme.radius
        border.width: 1
    }

    header: Rectangle {
        height: 44
        color: Theme.header
        border.color: Theme.borderSoft

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: root.title
                color: Theme.text
                font.bold: true
                font.pixelSize: 15
                elide: Text.ElideRight
            }

            ActionButton {
                text: "Fechar"
                implicitWidth: 84
                onClicked: {
                    root.viewModel.columnFlow.discardColumnSettings()
                    root.close()
                }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.cardGap

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 12
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
                    readonly property var themeOptions: ["system", "light", "dark", "gruvbox"]

                    Layout.preferredWidth: 150
                    model: themeOptions
                    currentIndex: Math.max(0, themeOptions.indexOf(root.viewModel.ui.theme))
                    onActivated: root.viewModel.ui.theme = currentText
                }

                Label {
                    text: "Densidade"
                    color: Theme.text
                    font.bold: true
                }
                AppComboBox {
                    readonly property var densityOptions: ["compact", "normal", "comfortable"]

                    Layout.preferredWidth: 160
                    model: densityOptions
                    currentIndex: Math.max(0, densityOptions.indexOf(root.viewModel.ui.density))
                    onActivated: root.viewModel.ui.density = currentText
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
                    stepSize: 20
                    Layout.preferredWidth: 120
                    onValueModified: root.viewModel.ui.detailsPanelWidth = value
                }

                Label {
                    Layout.columnSpan: 4
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
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            viewModel: root.viewModel.columns
        }
    }

    footer: Rectangle {
        height: 52
        color: Theme.header
        border.color: Theme.borderSoft

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

            Item { Layout.fillWidth: true }

            ActionButton {
                text: "Salvar"
                implicitWidth: 104
                onClicked: {
                    root.viewModel.columnFlow.applyColumnSettings()
                    root.close()
                }
            }
            ActionButton {
                text: "Descartar"
                implicitWidth: 112
                onClicked: {
                    root.viewModel.columnFlow.discardColumnSettings()
                    root.close()
                }
            }
        }
    }
}
