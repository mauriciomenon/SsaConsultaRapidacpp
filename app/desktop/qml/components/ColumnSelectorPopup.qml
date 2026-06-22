pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Popup {
    id: root
    required property var viewModel

    width: 760
    height: 560
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Colunas"
                color: Theme.text
                font.bold: true
                font.pixelSize: 15
                elide: Text.ElideRight
            }
            ActionButton {
                text: "Fechar"
                implicitWidth: 92
                onClicked: root.close()
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
                text: "Aplicar"
                implicitWidth: 104
                onClicked: root.viewModel.columnFlow.applyColumnSettings()
            }
            ActionButton {
                text: "Reverter"
                implicitWidth: 110
                onClicked: root.viewModel.columnFlow.discardColumnSettings()
            }
        }
    }
}
