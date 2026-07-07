pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SsaConsultaRapida

// Flow-based grid so each cell can carry its own height (the Macro card needs
// up to 132px for its report table; GridView enforces a uniform cellHeight and
// cannot host heterogeneous cards). Mirrors the previous GridView cellWidth
// formula so text filter cards keep their layout.
Item {
    id: root
    required property var filterViewModel
    required property var textFilters
    required property var advanced
    signal applyRequested

    Layout.fillWidth: true
    Layout.preferredHeight: gridFlow.childrenRect.height
    readonly property int cellWidth: width >= 960 ? Math.floor(width / 4) : (width >= 720 ? Math.floor(width / 3) : (width >= 520 ? Math.floor(width / 2) : width))
    // Text and reprogramming cards share the same compact height so their
    // titles land on the same baseline. Macro grows only when its report
    // table is visible.
    readonly property int textCellHeight: 56
    readonly property int macroCellHeight: root.advanced.macro.reportRows.length > 0 ? 132 : textCellHeight - 4

    function preloadOptions() {
        root.filterViewModel.preloadAdvancedColumnValueOptions();
    }

    Component.onCompleted: root.preloadOptions()

    Connections {
        target: root.filterViewModel
        function onColumnValueOptionsReset() {
            root.preloadOptions();
        }
    }

    Flow {
        id: gridFlow
        anchors.fill: parent
        spacing: 0

        // Text filter cards (14 cells: Setor emissor, ..., Situacao parcial).
        Repeater {
            model: root.textFilters.cardStates

            AdvancedTextFilterCard {
                id: textDelegate
                required property var modelData
                readonly property var rowData: textDelegate.modelData
                property var loadedValueOptions: []
                property var visibleValueOptions: []
                property bool moreValueOptions: false
                property bool valueOptionsLoading: false

                function reloadOptionState() {
                    loadedValueOptions = root.filterViewModel.columnValueOptionsFor(rowData.key);
                    valueOptionsLoading = root.filterViewModel.columnValueOptionsLoadingFor(rowData.key);
                    moreValueOptions = root.filterViewModel.hasMoreColumnValueOptionsFor(rowData.key, compactValueLimit);
                    visibleValueOptions = root.filterViewModel.columnValuePreviewOptionsFor(rowData.key, compactValueLimit, false);
                }

                cardWidth: root.cellWidth - 4
                cardHeight: root.textCellHeight - 4
                row: textDelegate.rowData
                operatorModes: root.textFilters.operatorModes
                allValues: loadedValueOptions
                visibleValues: visibleValueOptions
                hasMoreValues: moreValueOptions
                valuesLoading: valueOptionsLoading
                textFilter: rowData.textFilter !== undefined ? rowData.textFilter : ""
                operatorIndex: rowData.operatorIndex !== undefined ? rowData.operatorIndex : -1
                operatorLabel: rowData.operatorLabel !== undefined ? rowData.operatorLabel : ""

                Component.onCompleted: reloadOptionState()

                Connections {
                    target: root.filterViewModel
                    function onColumnValueOptionsChangedFor(key) {
                        if (key === textDelegate.rowData.key)
                            textDelegate.reloadOptionState();
                    }
                    function onColumnValueOptionsReset() {
                        textDelegate.reloadOptionState();
                    }
                }

                onOptionsRequested: {
                    if (loadedValueOptions.length === 0 && !valueOptionsLoading)
                        root.filterViewModel.refreshColumnValueOptionsFor(rowData.key);
                }
                onOperatorModeRequested: function (mode) {
                    root.textFilters.setOperatorMode(rowData.key, mode);
                }
                onSelectedValueRequested: function (value) {
                    root.textFilters.updateFilterWithSelectedValue(rowData.key, value);
                }
                onLoadedValuesReplacementRequested: function (mode) {
                    root.textFilters.replaceWithOperatorValueList(rowData.key, loadedValueOptions, mode);
                }
                onMixedValuesReplacementRequested: function (includeValues, excludeValues) {
                    root.textFilters.replaceWithOperatorValueLists(rowData.key, includeValues, excludeValues);
                }
                onTextFilterClearRequested: root.textFilters.clearTextFilterAndApply(rowData.key)
            }
        }

        // Macro card (single cell, after Situacao parcial).
        Repeater {
            model: 1

            AdvancedMacroFilterCard {
                required property int index
                cardWidth: root.cellWidth - 4
                cardHeight: root.macroCellHeight
                sectorHierarchy: root.advanced.sectorHierarchy
                macro: root.advanced.macro
                onApplyRequested: root.applyRequested()
            }
        }

        // Reprogramming card (single cell, after Macro).
        Repeater {
            model: 1

            AdvancedReprogrammingFilterCard {
                required property int index
                cardWidth: root.cellWidth - 4
                cardHeight: root.textCellHeight - 4
                filterViewModel: root.filterViewModel
                derivation: root.advanced.derivation
                onApplyRequested: root.applyRequested()
            }
        }

        // Emission week card (single cell). YYYYWW range on semana_cadastro.
        Repeater {
            model: 1

            AdvancedWeekEmissionCard {
                required property int index
                cardWidth: root.cellWidth - 4
                cardHeight: root.textCellHeight - 4
                week: root.advanced.week
                onApplyRequested: root.applyRequested()
            }
        }

        // Execution week card (single cell). YYYYWW range on semana_executada.
        Repeater {
            model: 1

            AdvancedWeekExecutionCard {
                required property int index
                cardWidth: root.cellWidth - 4
                cardHeight: root.textCellHeight - 4
                week: root.advanced.week
                onApplyRequested: root.applyRequested()
            }
        }
    }
}
