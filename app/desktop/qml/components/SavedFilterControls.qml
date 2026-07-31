pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Item {
    id: root
    required property var viewModel
    required property var filterViewModel
    required property var preferenceFlow
    property string density: "normal"
    property real savedFiltersMaximumWidth: 0
    property bool inlineSavedFilters: true
    property bool compactMenu: false
    signal exportRequested
    signal saveFiltersRequested
    signal exportFiltersRequested
    signal importFiltersRequested

    implicitWidth: controls.implicitWidth
    implicitHeight: controls.implicitHeight

    readonly property int savedFilterCount: preferenceFlow === null ? 0 : preferenceFlow.savedFilters.length

    function savedFilterChipWidth(filterName) {
        return Math.min(120, Math.max(64, Math.ceil(filterName.length * Theme.fontSizeMicro * 0.62) + 42));
    }

    function visibleSavedFilterCount() {
        const availableWidth = savedFilterStrip.width;
        const overflowWidth = 22;
        let usedWidth = 0;
        let visibleCount = 0;
        for (let index = 0; index < savedFilterCount; ++index) {
            const filter = preferenceFlow.savedFilters[index];
            const filterName = filter.name !== undefined ? String(filter.name) : "";
            const spacing = visibleCount > 0 ? 5 : 0;
            const reserveOverflow = index < savedFilterCount - 1 ? overflowWidth + 5 : 0;
            const nextWidth = usedWidth + spacing + savedFilterChipWidth(filterName) + reserveOverflow;
            if (nextWidth > availableWidth)
                break;
            usedWidth += spacing + savedFilterChipWidth(filterName);
            visibleCount += 1;
        }
        return visibleCount;
    }

    function openSaveFilterDialog() {
        if (root.preferenceFlow === null)
            return;
        if (!root.preferenceFlow.hasActiveFilter()) {
            root.preferenceFlow.notifyNoActiveFilter();
            return;
        }
        savedFilterName.text = root.preferenceFlow.suggestedFilterName();
        saveFilterDialog.open();
        savedFilterName.forceActiveFocus();
        savedFilterName.selectAll();
    }

    RowLayout {
        id: controls
        anchors.fill: parent
        spacing: Theme.gap

        ActionButton {
            id: filterMenuButton
            objectName: "mainFiltersButton"
            text: root.compactMenu ? "..." : "Filtros"
            implicitWidth: root.compactMenu ? 30 : 96
            Layout.preferredHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
            ToolTip.visible: hovered
            ToolTip.text: "Filtros"
            Accessible.name: "Filtros"
            onClicked: filterMenu.open()

            Menu {
                id: filterMenu
                y: filterMenuButton.height

                MenuItem {
                    text: "Salvar Filtro"
                    onTriggered: root.openSaveFilterDialog()
                }
                MenuSeparator {
                    visible: root.savedFilterCount > 0
                }
                Repeater {
                    model: root.savedFilterCount > 0 ? root.preferenceFlow.savedFilters : []

                    delegate: MenuItem {
                        required property var modelData
                        readonly property string filterName: modelData.name !== undefined ? modelData.name : ""
                        text: "Aplicar: " + filterName
                        onTriggered: root.preferenceFlow.applySavedFilter(filterName)
                    }
                }
                MenuSeparator {
                    visible: root.savedFilterCount > 0
                }
                Repeater {
                    model: root.savedFilterCount > 0 ? root.preferenceFlow.savedFilters : []

                    delegate: MenuItem {
                        required property var modelData
                        readonly property string filterName: modelData.name !== undefined ? modelData.name : ""
                        text: "Remover: " + filterName
                        onTriggered: root.preferenceFlow.removeSavedFilter(filterName)
                    }
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
                    onTriggered: root.filterViewModel.resetFilters()
                }
            }
        }

        Item {
            id: savedFilterStrip
            objectName: "savedFilterStrip"
            visible: root.inlineSavedFilters && root.savedFilterCount > 0
            Layout.preferredWidth: root.inlineSavedFilters ? Math.max(0, root.savedFiltersMaximumWidth) : 0
            Layout.maximumWidth: root.inlineSavedFilters ? Math.max(0, root.savedFiltersMaximumWidth) : 0
            Layout.preferredHeight: Theme.densityValue(root.density, 26, Theme.controlHeight, 34)
            clip: true

            Row {
                id: savedFilterTags
                height: parent.height
                spacing: 5

                Repeater {
                    model: root.preferenceFlow.savedFilters

                    Control {
                        id: savedFilterTag
                        required property int index
                        objectName: "savedFilterTag-" + index
                        required property var modelData
                        readonly property string filterName: modelData.name !== undefined ? modelData.name : ""
                        readonly property color effectiveBackground: savedFilterMouse.containsMouse ? Theme.accentSoft : Theme.panelRaised
                        visible: index < root.visibleSavedFilterCount()
                        implicitWidth: root.savedFilterChipWidth(filterName)
                        implicitHeight: 26
                        padding: 0
                        hoverEnabled: true
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: "Aplicar filtro salvo " + savedFilterTag.filterName
                        Accessible.onPressAction: savedFilterTag.applyFilter()
                        Keys.onReturnPressed: savedFilterTag.applyFilter()
                        Keys.onEnterPressed: savedFilterTag.applyFilter()
                        Keys.onSpacePressed: savedFilterTag.applyFilter()

                        function applyFilter() {
                            root.preferenceFlow.applySavedFilter(savedFilterTag.filterName);
                        }

                        ToolTip.visible: hovered && filterName.length > 0
                        ToolTip.text: filterName
                        ToolTip.delay: 0

                        background: Rectangle {
                            color: savedFilterTag.effectiveBackground
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
                                textFormat: Text.PlainText
                                color: Theme.readableText(savedFilterTag.effectiveBackground, Theme.text)
                                font.pixelSize: Theme.fontSizeMicro
                                font.bold: false
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
                                font.bold: false
                                palette.buttonText: Theme.readableText(removeSavedFilter.hovered ? Theme.accentSoft : savedFilterTag.effectiveBackground, Theme.text)
                                ToolTip.visible: hovered
                                ToolTip.text: "Remover filtro salvo"
                                Accessible.name: "Remover filtro salvo " + savedFilterTag.filterName
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
                            onClicked: {
                                savedFilterTag.forceActiveFocus();
                                savedFilterTag.applyFilter();
                            }
                        }
                    }
                }

                ToolButton {
                    id: savedFilterOverflowButton
                    objectName: "savedFilterOverflowButton"
                    visible: root.visibleSavedFilterCount() < root.savedFilterCount
                    width: 22
                    height: 22
                    text: "..."
                    padding: 0
                    font.pixelSize: Theme.fontSizeMicro
                    font.bold: false
                    ToolTip.visible: hovered
                    ToolTip.text: "Mais filtros salvos"
                    Accessible.name: "Mais filtros salvos"
                    onClicked: savedFilterOverflowMenu.open()

                    background: Rectangle {
                        color: savedFilterOverflowButton.hovered ? Theme.accentSoft : Theme.panelRaised
                        border.color: Theme.border
                        radius: Theme.radius
                    }

                    Menu {
                        id: savedFilterOverflowMenu
                        y: savedFilterOverflowButton.height

                        Repeater {
                            model: root.preferenceFlow.savedFilters

                            delegate: MenuItem {
                                required property var modelData
                                readonly property string filterName: modelData.name !== undefined ? modelData.name : ""
                                text: filterName
                                onTriggered: root.preferenceFlow.applySavedFilter(filterName)
                            }
                        }

                        MenuSeparator {}

                        Repeater {
                            model: root.preferenceFlow.savedFilters

                            delegate: MenuItem {
                                required property var modelData
                                readonly property string filterName: modelData.name !== undefined ? modelData.name : ""
                                text: "Remover: " + filterName
                                onTriggered: root.preferenceFlow.removeSavedFilter(filterName)
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: saveFilterDialog
        parent: Overlay.overlay
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
