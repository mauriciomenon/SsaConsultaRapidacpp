pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

GridView {
    id: root
    required property var filterViewModel
    required property var textFilters
    readonly property var emptyValues: []

    Layout.fillWidth: true
    Layout.preferredHeight: Math.max(cellHeight, contentHeight)
    Layout.minimumWidth: 0
    clip: true
    interactive: false
    model: root.textFilters.cardStates
    cellWidth: width >= 960 ? Math.floor(width / 4) : (width >= 720 ? Math.floor(width / 3) : (width >= 520 ? Math.floor(width / 2) : width))
    cellHeight: 56

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

    delegate: AdvancedTextFilterCard {
        id: filterCardDelegate
        required property var modelData
        property var loadedValueOptions: root.emptyValues
        property var visibleValueOptions: root.emptyValues
        property bool moreValueOptions: false
        property bool valueOptionsLoading: false

        function reloadOptionState() {
            loadedValueOptions = root.filterViewModel.columnValueOptionsFor(modelData.key);
            valueOptionsLoading = root.filterViewModel.columnValueOptionsLoadingFor(modelData.key);
            moreValueOptions = root.filterViewModel.hasMoreColumnValueOptionsFor(modelData.key, compactValueLimit);
            visibleValueOptions = root.filterViewModel.columnValuePreviewOptionsFor(modelData.key, compactValueLimit, false);
        }

        row: modelData
        operatorModes: root.textFilters.operatorModes
        allValues: loadedValueOptions
        visibleValues: visibleValueOptions
        hasMoreValues: moreValueOptions
        valuesLoading: valueOptionsLoading
        textFilter: modelData.textFilter
        operatorIndex: modelData.operatorIndex
        operatorLabel: modelData.operatorLabel
        cardWidth: root.cellWidth - 4
        cardHeight: root.cellHeight - 4

        Component.onCompleted: reloadOptionState()

        Connections {
            target: root.filterViewModel
            function onColumnValueOptionsChangedFor(key) {
                if (key === filterCardDelegate.modelData.key)
                    filterCardDelegate.reloadOptionState();
            }
            function onColumnValueOptionsReset() {
                filterCardDelegate.reloadOptionState();
            }
        }

        onOptionsRequested: {
            if (loadedValueOptions.length === 0 && !valueOptionsLoading)
                root.filterViewModel.refreshColumnValueOptionsFor(modelData.key);
        }
        onOperatorModeRequested: function (mode) {
            root.textFilters.setOperatorMode(modelData.key, mode);
        }
        onSelectedValueRequested: function (value) {
            root.textFilters.updateFilterWithSelectedValue(modelData.key, value);
        }
        onLoadedValuesReplacementRequested: function (mode) {
            root.textFilters.replaceWithOperatorValueList(modelData.key, loadedValueOptions, mode);
        }
        onMixedValuesReplacementRequested: function (includeValues, excludeValues) {
            root.textFilters.replaceWithOperatorValueLists(modelData.key, includeValues, excludeValues);
        }
        onTextFilterClearRequested: root.textFilters.clearTextFilterAndApply(modelData.key)
    }
}
