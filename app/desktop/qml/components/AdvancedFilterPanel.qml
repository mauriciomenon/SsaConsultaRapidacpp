pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property var advanced
    signal applyRequested()

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ScrollView {
        anchors.fill: parent
        anchors.margins: 12
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            AdvancedTextFilterGrid {
                filterViewModel: root.filterViewModel
                textFilters: root.advanced.text
            }

            AdvancedScopeFilterCard {
                filterViewModel: root.filterViewModel
                derivation: root.advanced.derivation
                onApplyRequested: root.applyRequested()
            }

            AdvancedWeekFilterCard {
                week: root.advanced.week
                advanced: root.advanced
                onApplyRequested: root.applyRequested()
            }
        }
    }
}
