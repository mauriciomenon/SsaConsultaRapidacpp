pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property var columnViewModel
    property int focusRequest: filterViewModel.focusColumnRequest
    readonly property string filterPlaceholder: "Filtro"
    readonly property string filterSyntaxHint: "Virgula combina alternativas; ! exclui; = exato; ^ inicio; $ fim; ~ contem"

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

            function rowIndexForKey(key) {
                for (let index = 0; index < count; ++index) {
                    const row = model[index];
                    if (row && row.key === key) {
                        return index;
                    }
                }
                return -1;
            }

            function focusRowForKey(key) {
                const index = rowIndexForKey(key);
                if (index < 0) {
                    return;
                }
                currentIndex = index;
                positionViewAtIndex(index, ListView.Contain);
                Qt.callLater(function () {
                    const item = itemAtIndex(index);
                    if (item) {
                        item.focusInput();
                    }
                });
            }

            delegate: ColumnFilterRow {
                required property var modelData

                width: columnFilterList.width
                row: modelData
                placeholderText: root.filterPlaceholder
                onFilterSubmitted: (key, value) => root.columnViewModel.applyFilterFor(key, value)
                onFilterCleared: key => root.columnViewModel.clearFilterFor(key)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    Layout.fillWidth: true
                    text: root.columnViewModel.activeFilterCount === 0 ? "Sem filtros por coluna" : root.columnViewModel.activeFilterCount + " filtros por coluna"
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontSizeBody
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.columnViewModel.activeFilterCount > 0
                    text: root.filterSyntaxHint
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontSizeCaption
                    elide: Text.ElideRight
                }
            }

            ActionButton {
                text: "Limpar todos"
                implicitWidth: 120
                onClicked: root.filterViewModel.resetFilters()
            }
        }
    }
    onFocusRequestChanged: {
        if (root.filterViewModel.columnKey.length > 0) {
            columnFilterList.focusRowForKey(root.filterViewModel.columnKey);
        }
    }
}
