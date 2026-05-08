import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel

    height: 48
    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap
        spacing: Theme.gap

        Label { text: "Pesquisa Geral:"; color: Theme.text }
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

        ToolSeparator {}
        ActionButton { text: "<"; onClicked: root.viewModel.previousPage() }
        Label {
            text: "Pagina " + root.viewModel.pageIndex + " de " + root.viewModel.pageCount
            color: Theme.text
        }
        ActionButton { text: ">"; onClicked: root.viewModel.nextPage() }
        SpinBox {
            from: 10
            to: 500
            stepSize: 10
            value: root.viewModel.pageSize
            onValueModified: root.viewModel.pageSize = value
        }
    }
}
