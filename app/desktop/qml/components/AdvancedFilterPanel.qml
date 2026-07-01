pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property var advanced
    signal applyRequested

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius

    ScrollView {
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 6

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                AdvancedScopeFilterCard {
                    Layout.fillWidth: true
                    filterViewModel: root.filterViewModel
                    derivation: root.advanced.derivation
                    onApplyRequested: root.applyRequested()
                }

                AdvancedMacroFilterCard {
                    sectorHierarchy: root.advanced.sectorHierarchy
                    macro: root.advanced.macro
                    onApplyRequested: root.applyRequested()
                }
            }

            AdvancedTextFilterGrid {
                filterViewModel: root.filterViewModel
                textFilters: root.advanced.text
            }

            AdvancedWeekFilterCard {
                week: root.advanced.week
                advanced: root.advanced
                onApplyRequested: root.applyRequested()
            }
        }
    }
}
