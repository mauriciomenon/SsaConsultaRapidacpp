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

    // Opens the save-filter dialog only when there is an active filter to
    // save (mirrors the Python reference). If nothing is active, the dialog
    // is not opened and the status message tells the user why.
    function openSaveFilterDialog() {
        if (root.preferenceFlow === null || !root.preferenceFlow.hasActiveFilter()) {
            root.preferenceFlow.notifyNoActiveFilter();
            return;
        }
        savedFilterName.text = root.preferenceFlow.suggestedFilterName();
        saveFilterDialog.open();
        savedFilterName.forceActiveFocus();
        savedFilterName.selectAll();
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
                font.pixelSize: Theme.fontSizeBody
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
                                    font.pixelSize: Theme.fontSizeMicro
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
                                    font.pixelSize: Theme.fontSizeMicro
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
                        text: "Salvar Filtro"
                        onTriggered: root.openSaveFilterDialog()
                    }
                    MenuSeparator {}
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
                    readonly property real minimumShortcutWidth: 38
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
                                    // Depend on a NOTIFY-backed property so this re-evaluates after
                                    // toggleStatusShortcut -> synchronizeFilterState -> changed().
                                    // Q_INVOKABLE alone does not invalidate the binding.
                                    readonly property string statusBindingKey: root.filterViewModel.activeFilterSummary
                                    // 0 = None, 1 = Included (=CODE), 2 = Excluded (!CODE).
                                    readonly property int shortcutState: {
                                        void statusBindingKey;
                                        return root.filterViewModel.statusShortcutState(modelData);
                                    }
                                    readonly property bool included: shortcutState === 1
                                    readonly property bool excluded: shortcutState === 2
                                    width: statusShortcutFrame.fittedShortcutWidth
                                    height: Theme.statusShortcutHeight
                                    text: modelData
                                    checkable: false
                                    padding: 0
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMicro
                                    // Excluded (!) keeps normal weight; only included is bold.
                                    font.bold: included
                                    onClicked: root.filterViewModel.toggleStatusShortcut(modelData)
                                    ToolTip.visible: hovered
                                    ToolTip.text: included ? qsTr("Incluindo %1 - clicar exclui").arg(modelData) : excluded ? qsTr("Excluindo %1 - clicar remove").arg(modelData) : qsTr("Filtrar situacao %1").arg(modelData)

                                    background: Rectangle {
                                        // Excluded: same fill as none, Theme.mutedText border only (no danger/strikeout).
                                        color: statusShortcut.included ? Theme.accent : statusShortcut.hovered ? Theme.accentSoft : Theme.panelRaised
                                        border.color: statusShortcut.included ? Theme.accentStrong : statusShortcut.excluded ? Theme.mutedText : Theme.border
                                        radius: Theme.radius
                                    }

                                    contentItem: Text {
                                        text: statusShortcut.text
                                        color: statusShortcut.included ? Theme.accentText : Theme.text
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
                font.pixelSize: Theme.fontSizeBody
                font.bold: false
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
            AppComboBox {
                id: sectorFilter

                Layout.preferredWidth: 104
                model: root.filterViewModel.sector.selectorValues
                currentIndex: root.filterViewModel.sector.selectorIndex
                displayText: root.filterViewModel.sector.quickSector.length > 0 ? root.filterViewModel.sector.quickSector : "Todos"
                implicitHeight: Theme.controlHeight - 4
                leftPadding: 6
                rightPadding: 18
                font.pixelSize: Theme.fontSizeMicro
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
        width: 360

        contentItem: ColumnLayout {
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Nome do filtro"
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
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
