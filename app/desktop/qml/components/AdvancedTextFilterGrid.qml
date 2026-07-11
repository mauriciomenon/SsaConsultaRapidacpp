pragma ComponentBehavior: Bound

import QtQuick
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

    implicitHeight: gridFlow.childrenRect.height
    height: implicitHeight
    // Single source of truth: Theme.resolveGridLayout picks columns, gap and
    // cardWidth together so cards fill the row with no trailing empty space.
    readonly property var gridLayout: Theme.resolveGridLayout(root.width)
    // Text and reprogramming cards share the same compact height so their
    // titles land on the same baseline. Macro grows only when its report
    // table is visible.
    readonly property int textCellHeight: 56
    readonly property int macroCellHeight: root.advanced.macro.reportRows.length > 0 ? 132 : textCellHeight - 4

    function prepareSmokeValues(columnKey, values) {
        for (var index = 0; index < gridFlow.children.length; ++index) {
            const child = gridFlow.children[index];
            if (child.key === columnKey)
                return child.prepareSmokeValues(values);
        }
        return null;
    }

    function smokeGeometry() {
        return {
            height: root.height,
            contentHeight: gridFlow.childrenRect.height
        };
    }

    Flow {
        id: gridFlow
        anchors.fill: parent
        spacing: root.gridLayout.spacing

        // Text filter cards (14 cells: Setor emissor, ..., Situacao parcial).
        Repeater {
            model: root.textFilters

            AdvancedTextFilterCard {
                id: textDelegate
                property var loadedValueOptions: []
                property bool valueOptionsLoading: false
                property int loadedMaxValueLength: 0

                function reloadOptionState() {
                    loadedValueOptions = root.filterViewModel.columnValueOptionsFor(textDelegate.key);
                    valueOptionsLoading = root.filterViewModel.columnValueOptionsLoadingFor(textDelegate.key);
                    loadedMaxValueLength = root.filterViewModel.columnValueMaxLengthFor(textDelegate.key);
                }

                cardWidth: root.gridLayout.cardWidth
                cardHeight: root.textCellHeight - 4
                operatorModes: root.textFilters.operatorModes
                allValues: loadedValueOptions
                visibleValues: loadedValueOptions
                valuesLoading: valueOptionsLoading
                maxValueLength: loadedMaxValueLength

                Component.onCompleted: reloadOptionState()

                Connections {
                    target: root.filterViewModel
                    function onColumnValueOptionsChangedFor(key) {
                        if (key === textDelegate.key)
                            textDelegate.reloadOptionState();
                    }
                    function onColumnValueOptionsReset() {
                        textDelegate.reloadOptionState();
                    }
                }

                onOptionsRequested: {
                    if (loadedValueOptions.length === 0 && !valueOptionsLoading)
                        root.filterViewModel.refreshColumnValueOptionsFor(textDelegate.key);
                }
                onOperatorModeRequested: function (mode) {
                    root.textFilters.setOperatorMode(textDelegate.key, mode);
                }
                onSelectedValueRequested: function (value) {
                    root.textFilters.updateFilterWithSelectedValue(textDelegate.key, value);
                }
                onMixedValuesReplacementRequested: function (includeValues, excludeValues) {
                    root.textFilters.replaceWithOperatorValueLists(textDelegate.key, includeValues, excludeValues);
                }
                onTextFilterClearRequested: root.textFilters.clearTextFilterAndApply(textDelegate.key)
            }
        }

        // Macro card (single cell, after Situacao parcial).
        Repeater {
            model: 1

            AdvancedMacroFilterCard {
                required property int index
                cardWidth: root.gridLayout.cardWidth
                cardHeight: root.macroCellHeight
                macro: root.advanced.macro
                onApplyRequested: root.applyRequested()
            }
        }

        // Reprogramming card (single cell, after Macro).
        Repeater {
            model: 1

            AdvancedReprogrammingFilterCard {
                required property int index
                cardWidth: root.gridLayout.cardWidth
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
                cardWidth: root.gridLayout.cardWidth
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
                cardWidth: root.gridLayout.cardWidth
                cardHeight: root.textCellHeight - 4
                week: root.advanced.week
                onApplyRequested: root.applyRequested()
            }
        }
    }
}
