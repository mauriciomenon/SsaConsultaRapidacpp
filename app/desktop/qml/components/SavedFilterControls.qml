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
    signal exportRequested
    signal saveFiltersRequested
    signal exportFiltersRequested
    signal importFiltersRequested

    implicitWidth: controls.implicitWidth
    implicitHeight: controls.implicitHeight

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
                        required property int index
                        objectName: "savedFilterTag-" + index
                        required property var modelData
                        readonly property string filterName: modelData.name !== undefined ? modelData.name : ""
                        implicitWidth: Math.min(120, tagLabel.implicitWidth + removeSavedFilter.implicitWidth + 26)
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
                                textFormat: Text.PlainText
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
