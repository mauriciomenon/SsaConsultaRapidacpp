pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

RowLayout {
    id: root
    required property var viewModel
    required property var filterViewModel

    spacing: Theme.gap

    ActionButton {
        text: "<"
        Accessible.name: "Pagina anterior"
        Layout.preferredWidth: 22
        implicitWidth: 22
        implicitHeight: Theme.controlHeight - 4
        enabled: root.viewModel.pageNumber > 1
        onClicked: root.viewModel.previousPage()
    }
    ActionButton {
        text: ">"
        Accessible.name: "Proxima pagina"
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
                            readonly property color effectiveBackground: included ? Theme.accent : excluded ? Theme.mutedText : hovered ? Theme.accentSoft : Theme.panelRaised
                            readonly property color preferredForeground: included ? Theme.accentText : excluded ? Theme.panel : Theme.text
                            width: statusShortcutFrame.fittedShortcutWidth
                            height: Theme.statusShortcutHeight
                            text: modelData
                            checkable: false
                            padding: 0
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMicro
                            font.bold: false
                            onClicked: root.filterViewModel.toggleStatusShortcut(modelData)
                            ToolTip.visible: hovered
                            ToolTip.text: included ? qsTr("Incluindo %1 - clicar exclui").arg(modelData) : excluded ? qsTr("Excluindo %1 - clicar remove").arg(modelData) : qsTr("Filtrar situacao %1").arg(modelData)

                            background: Rectangle {
                                // Included: accent fill. Excluded: invert text/bg colors for
                                // clear visual distinction without extra borders/danger.
                                // None: default panel.
                                color: statusShortcut.effectiveBackground
                                border.color: statusShortcut.included ? Theme.accentStrong : Theme.border
                                radius: Theme.radius
                            }

                            contentItem: Text {
                                text: statusShortcut.text
                                // Excluded: text color inverts to the panel (dark on light bg).
                                color: Theme.readableText(statusShortcut.effectiveBackground, statusShortcut.preferredForeground)
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
        color: Theme.readableText(Theme.surface, Theme.accent)
        font.pixelSize: Theme.fontSizeBody
        font.bold: false
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }
    AppComboBox {
        id: sectorFilter

        objectName: "quickSectorFilter"
        Layout.preferredWidth: 104
        model: root.filterViewModel.sector.selectorValues
        currentIndex: root.filterViewModel.sector.selectorIndex
        displayText: root.filterViewModel.sector.hasMultipleSectorSelections ? "..." : root.filterViewModel.sector.quickSector.length > 0 ? root.filterViewModel.sector.quickSector : "Todos"
        implicitHeight: Theme.controlHeight - 4
        leftPadding: 6
        rightPadding: 18
        font.pixelSize: Theme.fontSizeMicro
        font.bold: false
        ToolTip.visible: hovered && root.filterViewModel.sector.optionsError.length > 0
        ToolTip.text: root.filterViewModel.sector.optionsError
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
    ActionButton {
        objectName: "quickSectorRetryButton"
        visible: root.filterViewModel.sector.optionsError.length > 0
        text: "!"
        Layout.preferredWidth: 24
        implicitWidth: 24
        implicitHeight: Theme.controlHeight - 4
        Accessible.name: "Tentar carregar setores novamente"
        ToolTip.visible: hovered
        ToolTip.text: root.filterViewModel.sector.optionsError
        onClicked: root.filterViewModel.retryQuickSectorOptions()
    }
}
