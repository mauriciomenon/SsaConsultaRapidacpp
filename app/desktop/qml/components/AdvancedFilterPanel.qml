pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property var advanced
    signal applyRequested

    color: Theme.panel
    border.color: "transparent"
    radius: 0

    function openSmokeValues(columnKey, values) {
        return textFilterGrid.openSmokeValues(columnKey, values);
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 6
        clip: true
        contentWidth: availableWidth

        AdvancedTextFilterGrid {
            id: textFilterGrid
            width: parent.width
            filterViewModel: root.filterViewModel
            textFilters: root.advanced.text
            advanced: root.advanced
            onApplyRequested: root.applyRequested()
        }
    }
}
