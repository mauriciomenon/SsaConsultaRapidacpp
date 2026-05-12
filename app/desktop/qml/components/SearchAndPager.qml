import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel

    Layout.preferredHeight: 82
    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                text: "Pesquisa geral"
                color: Theme.text
                Layout.preferredWidth: 110
            }
            TextField {
                Layout.fillWidth: true
                implicitHeight: Theme.controlHeight
                text: root.viewModel.search.text
                placeholderText: "Termos separados por virgula; ! exclui; ^, $, =, ~ alteram o modo"
                onTextChanged: root.viewModel.search.text = text
                onAccepted: root.viewModel.search.apply()
            }
            ActionButton { text: "Aplicar"; onClicked: root.viewModel.search.apply() }
            ActionButton { text: "Limpar"; onClicked: root.viewModel.search.clear() }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Pagina " + root.viewModel.pageNumber + " de " + root.viewModel.pageCount +
                      "    Linhas por pagina"
                color: Theme.mutedText
                elide: Text.ElideRight
            }
            SpinBox {
                from: 10
                to: 500
                stepSize: 10
                value: root.viewModel.pageSize
                onValueModified: root.viewModel.pageSize = value
            }
            ActionButton { text: "<"; onClicked: root.viewModel.previousPage() }
            ActionButton { text: ">"; onClicked: root.viewModel.nextPage() }
        }
    }
}
