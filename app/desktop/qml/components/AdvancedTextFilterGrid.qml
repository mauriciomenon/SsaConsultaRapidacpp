pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

GridView {
    id: root
    required property var filterViewModel
    required property var textFilters
    readonly property var emptyValues: []

    Layout.fillWidth: true
    Layout.preferredHeight: Math.min(360, Math.max(cellHeight, contentHeight))
    Layout.minimumWidth: 0
    clip: true
    model: root.textFilters.rows
    cellWidth: width >= 760 ? Math.floor(width / 2) : width
    cellHeight: 112

    delegate: AdvancedTextFilterCard {
        required property var modelData

        row: modelData
        operatorModes: root.textFilters.operatorModes
        visibleValues: root.filterViewModel.columnValueOptionsVersion >= 0
                       ? root.filterViewModel.columnValuePreviewOptionsFor(
                             modelData.key,
                             compactValueLimit,
                             expandedValues)
                       : root.emptyValues
        hasMoreValues: root.filterViewModel.hasMoreColumnValueOptionsFor(
                           modelData.key,
                           compactValueLimit)
        valuesLoading: root.filterViewModel.columnValueOptionsLoadingFor(modelData.key)
        textFilter: root.textFilters.textFilter(modelData.key)
        operatorIndex: root.textFilters.version >= 0
                       ? root.textFilters.operatorIndexFor(modelData.key)
                       : -1
        operatorLabel: root.textFilters.version >= 0
                       ? root.textFilters.operatorLabelFor(modelData.key)
                       : ""
        cardWidth: root.cellWidth - 8
        cardHeight: root.cellHeight - 8

        onOptionsRequested: {
            if (root.filterViewModel.columnValueOptionsFor(modelData.key).length === 0
                    && !root.filterViewModel.columnValueOptionsLoadingFor(modelData.key))
                root.filterViewModel.refreshColumnValueOptionsFor(modelData.key)
        }
        onOperatorModeRequested: function(mode) {
            root.textFilters.setOperatorMode(modelData.key, mode)
        }
        onSelectedValueRequested: function(value) {
            root.textFilters.addSelectedValue(modelData.key, value)
        }
        onLoadedValuesFilterRequested: function(mode) {
            root.textFilters.replaceWithOperatorValueList(
                modelData.key,
                root.filterViewModel.columnValueOptionsFor(modelData.key),
                mode)
        }
        onTextFilterClearRequested: root.textFilters.setTextFilter(modelData.key, "")
    }
}
