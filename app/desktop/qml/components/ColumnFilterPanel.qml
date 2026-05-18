pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property var columnViewModel
    readonly property string filterSyntaxHint: "Termos separados por virgula; ! exclui; ^, $, =, ~ alteram o modo"

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        ListView {
            id: columnFilterList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: root.columnViewModel.rows

            delegate: ColumnFilterRow {
                required property var modelData

                width: columnFilterList.width
                row: modelData
                placeholderText: root.filterSyntaxHint
                onFilterSubmitted: (key, value) => root.columnViewModel.applyFilterFor(key, value)
                onFilterCleared: key => root.columnViewModel.clearFilterFor(key)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: root.columnViewModel.activeFilterCount === 0
                      ? "Nenhum filtro por coluna ativo"
                      : root.columnViewModel.activeFilterCount + " filtros por coluna ativos"
                color: Theme.mutedText
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            ActionButton {
                text: "Limpar todos"
                implicitWidth: 120
                onClicked: root.filterViewModel.resetFilters()
            }
        }
    }
}
