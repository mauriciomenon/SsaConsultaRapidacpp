pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    required property var preferenceFlow
    property string density: "normal"
    property string currentWeekText: ""
    property string ssaCountText: ""
    property bool importEnabled: true
    readonly property var filterViewModel: viewModel.filters
    readonly property int compactControlHeight: Theme.densityValue(root.density, 24, 28, 32)
    signal importRequested
    signal preferencesRequested
    signal themeCycleRequested
    signal exportRequested
    signal saveFiltersRequested
    signal exportFiltersRequested
    signal importFiltersRequested
    focus: true

    function focusSearchInput() {
        if (root.visible) {
            searchInput.forceActiveFocus();
        }
    }

    function openSaveFilterDialog() {
        savedFilterControls.openSaveFilterDialog();
    }

    Component.onCompleted: Qt.callLater(root.focusSearchInput)
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(root.focusSearchInput);
        }
    }

    Layout.preferredHeight: Theme.densityValue(root.density, 126, 142, 158)
    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.densityValue(root.density, 6, 8, 10)
        spacing: Theme.densityValue(root.density, 5, Theme.gap, 10)

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Rectangle {
                id: searchGroup
                objectName: "mainSearchGroup"
                Layout.fillWidth: true
                Layout.minimumWidth: 260
                Layout.preferredHeight: root.compactControlHeight
                color: Theme.panelRaised
                border.color: searchInput.activeFocus ? Theme.accent : Theme.border
                radius: Theme.radius

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 1
                    anchors.rightMargin: 1
                    spacing: 2

                    ActionButton {
                        objectName: "mainUndoButton"
                        text: "↶"
                        enabled: root.viewModel.canUndoFilters
                        implicitWidth: 32
                        implicitHeight: root.compactControlHeight - 2
                        Accessible.name: "Desfazer"
                        ToolTip.visible: hovered
                        ToolTip.text: "Desfazer"
                        ToolTip.delay: 400
                        onClicked: root.viewModel.undoFilters()
                    }
                    AppTextField {
                        id: searchInput
                        objectName: "mainSearchInput"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                        implicitHeight: root.compactControlHeight - 2
                        text: root.viewModel.search.text
                        placeholderText: "Busca geral"
                        placeholderTextColor: Theme.mutedText
                        font.pixelSize: Theme.fontSizeBody
                        background: Rectangle {
                            color: "transparent"
                            border.width: 0
                        }
                        onTextEdited: root.viewModel.search.text = text
                        onAccepted: root.viewModel.search.apply()
                    }
                    ActionButton {
                        objectName: "mainClearButton"
                        text: "⌫"
                        implicitWidth: 32
                        implicitHeight: root.compactControlHeight - 2
                        Accessible.name: "Limpar"
                        ToolTip.visible: hovered
                        ToolTip.text: "Limpar"
                        ToolTip.delay: 400
                        onClicked: root.viewModel.search.clear()
                    }
                    ActionButton {
                        objectName: "mainApplyButton"
                        text: "⏎"
                        implicitWidth: 32
                        implicitHeight: root.compactControlHeight - 2
                        Accessible.name: "Aplicar"
                        ToolTip.visible: hovered
                        ToolTip.text: "Aplicar"
                        ToolTip.delay: 400
                        onClicked: root.viewModel.search.apply()
                    }
                }
            }
            Label {
                objectName: "mainWeekLabel"
                Layout.preferredHeight: root.compactControlHeight
                leftPadding: 8
                rightPadding: 8
                text: root.currentWeekText
                color: Theme.accent
                font.pixelSize: Theme.fontSizeLabel
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                background: Rectangle {
                    color: Theme.window
                    border.color: Theme.border
                    radius: Theme.radius
                }
            }
            Label {
                objectName: "mainSsaCountLabel"
                Layout.preferredHeight: root.compactControlHeight
                leftPadding: 8
                rightPadding: 8
                text: root.ssaCountText
                color: Theme.accent
                font.pixelSize: Theme.fontSizeMicro
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                background: Rectangle {
                    color: Theme.window
                    border.color: Theme.border
                    radius: Theme.radius
                }
            }
            ActionButton {
                objectName: "mainImportXlsxButton"
                text: "Importar XLSX"
                enabled: root.importEnabled
                padding: 8
                font.pixelSize: Theme.fontSizeLabel
                implicitWidth: implicitContentWidth + leftPadding + rightPadding
                implicitHeight: root.compactControlHeight
                onClicked: root.importRequested()
            }
            ActionButton {
                objectName: "mainPreferencesButton"
                text: "Preferencias"
                padding: 8
                font.pixelSize: Theme.fontSizeLabel
                implicitWidth: implicitContentWidth + leftPadding + rightPadding
                implicitHeight: root.compactControlHeight
                onClicked: root.preferencesRequested()
            }
            Button {
                objectName: "mainThemeButton"
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                implicitWidth: 28
                implicitHeight: 28
                padding: 0
                topInset: 0
                bottomInset: 0
                leftInset: 0
                rightInset: 0
                text: ""
                Accessible.name: "Alternar tema"
                ToolTip.text: qsTr("Tema")
                ToolTip.visible: hovered
                ToolTip.delay: 400
                background: Rectangle {
                    radius: 14
                    color: Theme.accent
                    border.color: Theme.border
                    border.width: 1
                }
                onClicked: root.themeCycleRequested()
            }
        }

        Rectangle {
            id: appliedFilterBar
            objectName: "mainAppliedFilterBar"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.densityValue(root.density, 30, 34, 38)
            color: "transparent"
            border.width: filterSummary.frameBorderWidth
            border.color: filterSummary.frameBorderColor
            radius: Theme.radius

            RowLayout {
                anchors.fill: parent
                anchors.margins: 3
                spacing: Theme.gap

                FilterSummaryBar {
                    id: filterSummary
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    framed: false
                    filterViewModel: root.filterViewModel
                    searchText: root.viewModel.search.text
                    onClearSearchRequested: root.viewModel.search.clear()
                    onClearAllRequested: {
                        root.viewModel.search.text = "";
                        root.filterViewModel.resetFilters();
                    }
                }

                SavedFilterControls {
                    id: savedFilterControls
                    Layout.preferredWidth: 96
                    Layout.maximumWidth: 96
                    viewModel: root.viewModel
                    filterViewModel: root.filterViewModel
                    preferenceFlow: root.preferenceFlow
                    density: root.density
                    inlineSavedFilters: false
                    onExportRequested: root.exportRequested()
                    onSaveFiltersRequested: root.saveFiltersRequested()
                    onExportFiltersRequested: root.exportFiltersRequested()
                    onImportFiltersRequested: root.importFiltersRequested()
                }
            }
        }

        PagerQuickFilters {
            Layout.fillWidth: true
            viewModel: root.viewModel
            filterViewModel: root.filterViewModel
        }
    }
}
