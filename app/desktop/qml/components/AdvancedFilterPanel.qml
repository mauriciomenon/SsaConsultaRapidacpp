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
    border.color: "transparent"
    radius: 0

    ScrollView {
        anchors.fill: parent
        anchors.margins: 6
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 5

            Flow {
                id: topFilterFlow
                Layout.fillWidth: true
                Layout.preferredHeight: childrenRect.height
                spacing: 5
                readonly property bool compactPair: width >= 960
                readonly property int scopeWidth: compactPair ? Math.min(650, width - 305) : width

                AdvancedScopeFilterCard {
                    width: topFilterFlow.compactPair ? topFilterFlow.scopeWidth : topFilterFlow.width
                    height: implicitHeight
                    filterViewModel: root.filterViewModel
                    derivation: root.advanced.derivation
                    onApplyRequested: root.applyRequested()
                }

                AdvancedMacroFilterCard {
                    width: topFilterFlow.compactPair ? topFilterFlow.width - topFilterFlow.scopeWidth - topFilterFlow.spacing : topFilterFlow.width
                    height: implicitHeight
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
