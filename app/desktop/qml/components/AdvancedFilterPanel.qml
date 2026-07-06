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
                // Mirrors AdvancedTextFilterGrid.cellWidth so the macro card's
                // right edge aligns with the last grid cell below.
                readonly property int textFilterCellWidth: width >= 960 ? Math.floor(width / 4) : (width >= 720 ? Math.floor(width / 3) : (width >= 520 ? Math.floor(width / 2) : width))
                readonly property int macroWidth: compactPair ? textFilterCellWidth : width
                readonly property int scopeWidth: compactPair ? width - macroWidth - spacing : width

                AdvancedScopeFilterCard {
                    width: topFilterFlow.scopeWidth
                    height: implicitHeight
                    filterViewModel: root.filterViewModel
                    derivation: root.advanced.derivation
                    onApplyRequested: root.applyRequested()
                }

                AdvancedMacroFilterCard {
                    width: topFilterFlow.macroWidth
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

            Flow {
                id: reprogrammingFlow
                Layout.fillWidth: true
                Layout.preferredHeight: childrenRect.height
                spacing: 5

                AdvancedReprogrammingFilterCard {
                    width: topFilterFlow.textFilterCellWidth
                    height: implicitHeight
                    filterViewModel: root.filterViewModel
                    derivation: root.advanced.derivation
                    onApplyRequested: root.applyRequested()
                }
            }

            AdvancedWeekFilterCard {
                week: root.advanced.week
                advanced: root.advanced
                onApplyRequested: root.applyRequested()
            }
        }
    }
}
