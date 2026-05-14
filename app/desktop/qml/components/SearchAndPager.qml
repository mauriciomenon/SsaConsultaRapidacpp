import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    focus: true

    Layout.preferredHeight: 112
    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.cardGap
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap
            Layout.preferredHeight: 36

            Rectangle {
                Layout.preferredWidth: 4
                Layout.fillHeight: true
                color: Theme.accent
                radius: 99
            }

            Label {
                font.pixelSize: 13
                color: Theme.mutedText
                font.bold: true
                text: "Busca"
                Layout.preferredWidth: 72
            }
            Label {
                Layout.fillWidth: true
                text: "Filtragem geral e paginacao"
                color: Theme.mutedText
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                text: "Pesquisa geral"
                font.pixelSize: 12
                color: Theme.text
                Layout.preferredWidth: 112
            }
            AppTextField {
                id: searchInput
                Layout.fillWidth: true
                text: root.viewModel.search.text
                placeholderText: "Termos separados por virgula; ! exclui; ^, $, =, ~ alteram o modo"
                placeholderTextColor: Theme.mutedText
                font.pixelSize: 12
                onTextChanged: root.viewModel.search.text = text
                onAccepted: {
                    root.viewModel.search.apply()
                }
                Component.onCompleted: forceActiveFocus()
            }
            ActionButton {
                text: "Limpar"
                onClicked: root.viewModel.search.clear()
            }
            ActionButton {
                text: "Aplicar agora"
                onClicked: root.viewModel.search.apply()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                font.pixelSize: 12
                text: (root.viewModel.totalRows === 0
                       ? "Sem resultados"
                       : "Pagina " + root.viewModel.pageNumber + " de " + root.viewModel.pageCount) +
                      "    Linhas por pagina"
                color: Theme.mutedText
                elide: Text.ElideRight
            }
            SpinBox {
                id: pageSizeSpin
                from: 10
                to: 500
                stepSize: 10
                value: root.viewModel.pageSize
                implicitHeight: Theme.controlHeight
                padding: 0
                down.indicator.width: 24
                up.indicator.width: 24
                onValueModified: root.viewModel.pageSize = value
                contentItem: TextInput {
                    text: pageSizeSpin.textFromValue(pageSizeSpin.value, pageSizeSpin.locale)
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    font.pixelSize: 12
                    color: Theme.text
                }
            }
            ActionButton {
                text: "Anterior"
                enabled: root.viewModel.totalRows > 0
                onClicked: root.viewModel.previousPage()
            }
            ActionButton {
                text: "Proxima"
                enabled: root.viewModel.totalRows > 0
                onClicked: root.viewModel.nextPage()
            }
        }
    }
}
