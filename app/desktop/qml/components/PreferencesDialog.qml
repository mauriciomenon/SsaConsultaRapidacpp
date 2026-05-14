pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Dialog {
    id: root
    required property var viewModel
    readonly property int hostAvailableWidth: parent && parent.width > 0
        ? parent.width
        : 1280
    readonly property int hostAvailableHeight: parent && parent.height > 0
        ? parent.height
        : 840
    readonly property int detailsCurrentWidth: root.viewModel.ui.detailsEffectiveWidth
    readonly property int detailsWidthMin: root.viewModel.ui.detailsMinimumWidth
    readonly property int detailsWidthMax: root.viewModel.ui.detailsMaximumWidth

    modal: true
    title: "Preferencias"
    width: Math.max(560, Math.min(820, hostAvailableWidth - 80))
    height: Math.max(460, Math.min(700, hostAvailableHeight - 80))
    x: Math.round((hostAvailableWidth - width) / 2)
    y: Math.round((hostAvailableHeight - height) / 2)
    closePolicy: Popup.NoAutoClose
    standardButtons: Dialog.NoButton

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.border
        radius: Theme.radius
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.cardGap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Rectangle {
                Layout.preferredWidth: 3
                Layout.fillHeight: true
                color: Theme.accent
            }

            Label {
                text: "Tema"
                color: Theme.text
                Layout.preferredWidth: 90
            }
            ComboBox {
                readonly property var themeOptions: ["system", "light", "dark", "gruvbox"]

                Layout.preferredWidth: 160
                model: themeOptions
                currentIndex: Math.max(0, themeOptions.indexOf(root.viewModel.ui.theme))
                onActivated: root.viewModel.ui.theme = currentText
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
                currentIndex: Math.max(0, densityOptions.indexOf(root.viewModel.ui.density))
                onActivated: root.viewModel.ui.density = currentText
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
            checked: root.viewModel.ui.detailsVisible
            onToggled: root.viewModel.ui.detailsVisible = checked
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
                id: detailsWidthInput
                from: root.detailsWidthMin
                to: root.detailsWidthMax
                value: root.detailsCurrentWidth
                stepSize: 20
                onValueModified: root.viewModel.ui.detailsPanelWidth = value
            }
            Label {
                Layout.fillWidth: true
                text: "pixels"
                color: Theme.mutedText
                elide: Text.ElideRight
            }
        }

        ColumnConfigList {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.viewModel.columns
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
