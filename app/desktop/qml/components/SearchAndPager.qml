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
    readonly property var filterViewModel: viewModel.filters
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

            ActionButton {
                text: "Limpar"
                implicitWidth: 88
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: {
                    root.viewModel.search.clear();
                    root.viewModel.search.apply();
                }
            }
            AppTextField {
                id: searchInput
                Layout.fillWidth: true
                text: root.viewModel.search.text
                placeholderText: "Busca geral"
                placeholderTextColor: Theme.mutedText
                font.pixelSize: 12
                onTextEdited: root.viewModel.search.text = text
                onAccepted: {
                    root.viewModel.search.apply();
                }
            }
            ActionButton {
                text: "Aplicar"
                implicitWidth: 82
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: root.viewModel.search.apply()
            }
            ActionButton {
                text: "Salvar Filtros"
                implicitWidth: 126
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: {
                    savedFilterName.text = root.preferenceFlow.suggestedFilterName();
                    saveFilterDialog.open();
                    savedFilterName.forceActiveFocus();
                    savedFilterName.selectAll();
                }
            }
            ScrollView {
                visible: root.preferenceFlow.savedFilters.length > 0
                Layout.preferredWidth: Math.min(300, savedFilterTags.implicitWidth + 4)
                Layout.maximumWidth: 300
                Layout.preferredHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                clip: true
                contentHeight: availableHeight
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: savedFilterTags.width > width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

                Row {
                    id: savedFilterTags
                    height: parent.availableHeight
                    spacing: 5

                    Repeater {
                        model: root.preferenceFlow.savedFilters

                        Control {
                            id: savedFilterTag
                            required property var modelData
                            readonly property string filterName: modelData.name !== undefined ? modelData.name : ""
                            implicitWidth: Math.min(120, tagLabel.implicitWidth + removeSavedFilter.implicitWidth + 26)
                            implicitHeight: 26
                            padding: 0
                            hoverEnabled: true

                            ToolTip.visible: hovered && filterName.length > 0
                            ToolTip.text: filterName
                            ToolTip.delay: 0

                            background: Rectangle {
                                color: savedFilterMouse.containsMouse ? Theme.accentSoft : Theme.panelRaised
                                border.color: Theme.border
                                radius: Theme.radius
                            }

                            contentItem: RowLayout {
                                spacing: 3

                                Label {
                                    id: tagLabel
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 6
                                    text: savedFilterTag.filterName
                                    color: Theme.text
                                    font.pixelSize: 11
                                    font.bold: true
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }

                                ToolButton {
                                    id: removeSavedFilter
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    text: "x"
                                    padding: 0
                                    font.pixelSize: 11
                                    font.bold: true
                                    palette.buttonText: Theme.accentStrong
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Remover filtro salvo"
                                    onClicked: root.preferenceFlow.removeSavedFilter(savedFilterTag.filterName)

                                    background: Rectangle {
                                        color: removeSavedFilter.hovered ? Theme.accentSoft : "transparent"
                                        radius: Theme.radius
                                    }
                                }
                            }

                            MouseArea {
                                id: savedFilterMouse
                                anchors.fill: parent
                                anchors.rightMargin: removeSavedFilter.width
                                hoverEnabled: true
                                onClicked: root.preferenceFlow.applySavedFilter(savedFilterTag.filterName)
                            }
                        }
                    }
                }
            }
            ActionButton {
                id: filterMenuButton
                text: "Filtros"
                implicitWidth: 96
                implicitHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
                onClicked: filterMenu.open()

                Menu {
                    id: filterMenu
                    y: filterMenuButton.height

                    MenuItem {
                        text: "Exportar Lista"
                        onTriggered: root.exportRequested()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Salvar Preferencias"
                        onTriggered: root.saveFiltersRequested()
                    }
                    MenuItem {
                        text: "Exportar Filtros"
                        onTriggered: root.exportFiltersRequested()
                    }
                    MenuItem {
                        text: "Importar Filtros"
                        onTriggered: root.importFiltersRequested()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Limpar Filtros"
                        onTriggered: {
                            root.filterViewModel.resetFilters();
                            root.viewModel.apply();
                        }
                    }
                }
            }
        }

        FilterSummaryBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.densityValue(root.density, 30, 34, 38)
            filterViewModel: root.filterViewModel
            searchText: root.viewModel.search.text
            onClearSearchRequested: {
                root.viewModel.search.clear();
                root.viewModel.search.apply();
            }
            onClearAllRequested: {
                root.viewModel.search.clear();
                root.filterViewModel.resetFilters();
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            ActionButton {
                text: "<"
                Layout.preferredWidth: 22
                implicitWidth: 22
                implicitHeight: Theme.controlHeight - 4
                enabled: root.viewModel.pageNumber > 1
                onClicked: root.viewModel.previousPage()
            }
            ActionButton {
                text: ">"
                Layout.preferredWidth: 22
                implicitWidth: 22
                implicitHeight: Theme.controlHeight - 4
                enabled: root.viewModel.pageNumber < root.viewModel.pageCount
                onClicked: root.viewModel.nextPage()
            }
            AppSpinBox {
                id: pageSizeSpin
                from: 10
                to: 500
                stepSize: 5
                value: root.viewModel.pageSize
                Layout.preferredWidth: 58
                onValueModified: {
                    root.viewModel.pageSize = pageSizeSpin.value;
                }
            }
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.controlHeight

                Rectangle {
                    id: statusShortcutFrame
                    readonly property int shortcutCount: root.filterViewModel.statusShortcutValues.length
                    readonly property real shortcutSpacing: 3
                    readonly property real preferredShortcutWidth: 48
                    readonly property real minimumShortcutWidth: 40
                    readonly property real fittedShortcutWidth: shortcutCount > 0 ? Math.max(minimumShortcutWidth, Math.min(preferredShortcutWidth, Math.floor((parent.width - 8 - shortcutSpacing * (shortcutCount - 1)) / shortcutCount))) : preferredShortcutWidth
                    readonly property real fittedContentWidth: shortcutCount > 0 ? fittedShortcutWidth * shortcutCount + shortcutSpacing * (shortcutCount - 1) + 8 : 0
                    anchors.centerIn: parent
                    width: Math.min(parent.width, fittedContentWidth)
                    height: Theme.controlHeight
                    color: "transparent"
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                    radius: Theme.radius
                    clip: true

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true
                        contentHeight: availableHeight
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                        ScrollBar.horizontal.policy: statusShortcutRow.width > width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

                        Row {
                            id: statusShortcutRow
                            height: parent.availableHeight
                            spacing: statusShortcutFrame.shortcutSpacing

                            Repeater {
                                model: root.filterViewModel.statusShortcutValues

                                Button {
                                    id: statusShortcut
                                    required property string modelData
                                    readonly property string filterState: root.filterViewModel.activeFilterSummary
                                    readonly property bool selected: filterState.length >= 0 && root.filterViewModel.statusShortcutSelected(modelData)
                                    width: statusShortcutFrame.fittedShortcutWidth
                                    height: 24
                                    text: modelData
                                    checkable: false
                                    padding: 0
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11
                                    font.bold: selected
                                    onClicked: root.filterViewModel.toggleStatusShortcut(modelData)
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Filtrar situacao " + modelData

                                    background: Rectangle {
                                        color: statusShortcut.selected ? Theme.accent : statusShortcut.hovered ? Theme.accentSoft : Theme.panelRaised
                                        border.color: statusShortcut.selected ? Theme.accentStrong : Theme.border
                                        radius: Theme.radius
                                    }

                                    contentItem: Text {
                                        text: statusShortcut.text
                                        color: statusShortcut.selected ? Theme.accentText : Theme.text
                                        font.family: statusShortcut.font.family
                                        font.pixelSize: statusShortcut.font.pixelSize
                                        font.bold: statusShortcut.font.bold
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Label {
                text: "Setor:"
                color: Theme.accent
                font.pixelSize: 12
                font.bold: false
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
            AppComboBox {
                id: sectorFilter

                Layout.preferredWidth: 65
                model: root.filterViewModel.sector.selectorValues
                currentIndex: root.filterViewModel.sector.selectorIndex
                displayText: root.filterViewModel.sector.quickSector.length > 0 ? root.filterViewModel.sector.quickSector : "Todos"
                implicitHeight: Theme.controlHeight - 4
                font.pixelSize: 11
                font.bold: false
                onActivated: {
                    root.filterViewModel.sector.quickSector = sectorFilter.currentText;
                    root.viewModel.apply();
                }
                delegate: ItemDelegate {
                    required property string modelData
                    width: sectorFilter.width
                    text: modelData.length === 0 ? "Todos" : modelData
                }
            }
        }
    }

    Dialog {
        id: saveFilterDialog
        title: "Salvar filtro"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay

        ColumnLayout {
            width: 360
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Nome do filtro"
                color: Theme.text
                font.pixelSize: 12
            }

            AppTextField {
                id: savedFilterName
                Layout.fillWidth: true
                placeholderText: "Nome"
                onAccepted: saveFilterDialog.accept()
            }
        }

        onAccepted: root.preferenceFlow.saveCurrentFilter(savedFilterName.text)
    }
}
