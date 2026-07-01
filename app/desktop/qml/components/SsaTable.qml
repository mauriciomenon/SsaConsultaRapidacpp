pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    property var columnSettings: null
    property var columnFlow: null
    required property string density
    signal openRequested
    signal configureColumnsRequested
    signal copyDerivationSvgRequested
    signal copyTextRequested(string text)
    signal navigateToRelationRequested(string ssaNumber)
    signal detailsWindowRequested

    readonly property int headerHeight: Theme.densityValue(root.density, 26, 30, 36)
    readonly property int rowHeight: Theme.densityValue(root.density, 25, 30, 35)
    readonly property int textSize: Theme.densityValue(root.density, 12, 13, 14)
    readonly property int fallbackColumnWidth: 120
    readonly property var tableColumns: root.viewModel.tableHeaders
    property int previewColumnIndex: -1
    property int previewColumnWidth: 0

    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    Column {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Flickable {
            id: header
            width: parent.width
            height: root.headerHeight
            contentWidth: headerRow.width
            clip: true
            interactive: false

            Row {
                id: headerRow
                height: parent.height
                spacing: 0

                Repeater {
                    model: root.tableColumns

                    delegate: Rectangle {
                        id: headerCell
                        required property int index
                        required property var modelData
                        readonly property bool hasColumnKey: modelData.key !== undefined && modelData.key !== null && modelData.key !== ""
                        readonly property string columnKey: hasColumnKey ? modelData.key : ""
                        readonly property int modelWidth: modelData.width !== undefined && modelData.width !== null ? modelData.width : root.fallbackColumnWidth
                        readonly property string effectiveLabel: {
                            const full = modelData.labelFull !== undefined ? modelData.labelFull : "";
                            return full.length > 0 ? full : (modelData.label !== undefined ? modelData.label : "");
                        }
                        property int dragStartWidth: 0
                        property real dragStartX: 0
                        property int previewWidth: modelWidth

                        width: previewWidth
                        height: header.height
                        color: Theme.tableHeader
                        border.color: Theme.borderSoft
                        border.width: 0

                        onModelDataChanged: previewWidth = modelWidth

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            text: headerCell.effectiveLabel + (headerCell.modelData.filtered ? " [f]" : "") + (headerCell.modelData.sorted ? (headerCell.modelData.sortAscending ? "  ^" : "  v") : "")
                            color: Theme.accentStrong
                            font.bold: true
                            font.pixelSize: root.textSize
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            anchors.rightMargin: 8
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function (mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    headerMenu.popup();
                                    return;
                                }
                                root.viewModel.sortByColumn(headerCell.index);
                            }
                        }

                        Menu {
                            id: headerMenu

                            MenuItem {
                                text: "Filtrar coluna"
                                enabled: headerCell.hasColumnKey
                                onTriggered: root.viewModel.setFilterPanelFocusColumn(headerCell.columnKey)
                            }
                            MenuItem {
                                text: "Ocultar coluna"
                                enabled: headerCell.hasColumnKey && root.columnFlow !== null && root.columnFlow.canHideColumn(headerCell.columnKey)
                                onTriggered: root.columnFlow.setColumnVisibleAndApply(headerCell.columnKey, false)
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: "Configurar colunas"
                                onTriggered: root.configureColumnsRequested()
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 8
                            visible: root.columnSettings !== null && root.columnFlow !== null && headerCell.hasColumnKey
                            color: resizeHandle.containsMouse || resizeHandle.pressed ? Theme.accentSoft : "transparent"

                            MouseArea {
                                id: resizeHandle
                                anchors.fill: parent
                                cursorShape: Qt.SizeHorCursor
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton

                                onPressed: function (mouse) {
                                    headerCell.dragStartWidth = headerCell.width;
                                    headerCell.dragStartX = resizeHandle.mapToItem(null, mouse.x, mouse.y).x;
                                    root.previewColumnIndex = headerCell.index;
                                    root.previewColumnWidth = headerCell.width;
                                }
                                onPositionChanged: function (mouse) {
                                    if (!pressed) {
                                        return;
                                    }
                                    if (root.columnSettings === null) {
                                        return;
                                    }
                                    const currentX = resizeHandle.mapToItem(null, mouse.x, mouse.y).x;
                                    const bounded = Math.max(root.columnSettings.minColumnWidth, Math.min(root.columnSettings.maxColumnWidth, headerCell.dragStartWidth + (currentX - headerCell.dragStartX)));
                                    headerCell.previewWidth = bounded;
                                    root.previewColumnWidth = bounded;
                                    table.forceLayout();
                                }
                                onReleased: {
                                    if (root.columnFlow !== null && headerCell.hasColumnKey) {
                                        const changed = root.columnFlow.setColumnWidthAndApply(headerCell.columnKey, headerCell.previewWidth);
                                        if (!changed) {
                                            headerCell.previewWidth = headerCell.modelWidth;
                                        }
                                    } else {
                                        headerCell.previewWidth = headerCell.modelWidth;
                                    }
                                    root.previewColumnIndex = -1;
                                    root.previewColumnWidth = 0;
                                    table.forceLayout();
                                }
                            }
                        }
                    }
                }
            }
        }

        TableView {
            id: table
            width: parent.width
            height: Math.max(0, parent.height - header.height)
            model: root.viewModel.tableModel
            clip: true
            rowSpacing: 0
            columnSpacing: 0
            boundsBehavior: Flickable.StopAtBounds
            columnWidthProvider: function (column) {
                if (column < 0 || column >= root.tableColumns.length) {
                    return 0;
                }
                if (column === root.previewColumnIndex) {
                    return root.previewColumnWidth;
                }
                const configuredWidth = root.tableColumns[column].width;
                return configuredWidth !== undefined && configuredWidth !== null ? configuredWidth : root.fallbackColumnWidth;
            }
            rowHeightProvider: function (row) {
                return table.cachedRowHeight;
            }
            readonly property int cachedRowHeight: root.rowHeight
            readonly property int cachedTextSize: root.textSize
            onContentXChanged: header.contentX = contentX
            ScrollBar.horizontal: ScrollBar {}
            ScrollBar.vertical: ScrollBar {}

            Timer {
                id: relayoutTimer
                interval: 33
                repeat: false
                onTriggered: table.forceLayout()
            }

            Connections {
                target: root.viewModel.tableModel

                function onColumnsChanged() {
                    relayoutTimer.restart();
                }
            }

            Connections {
                target: root

                function onDensityChanged() {
                    relayoutTimer.restart();
                }
            }

            delegate: Rectangle {
                id: cellDelegate
                required property int row
                required property int column
                required property var displayValue
                readonly property var columnConfig: root.tableColumns[column] ? root.tableColumns[column] : ({})
                readonly property bool isStriped: (row % 2) !== 0
                readonly property bool isSelected: row === root.viewModel.currentRow
                readonly property bool opensSam: columnConfig.opensSam === true
                readonly property string cellText: displayValue === undefined || displayValue === "" ? "-" : String(displayValue)
                readonly property string rowSsaNumber: root.viewModel.tableModel.ssaNumberAt(cellDelegate.row)
                readonly property bool isDerivationLink: columnConfig.key === "derivada_de" && cellText !== "-"
                readonly property bool opensDerivationGraph: columnConfig.key === "qtd_derivadas" && Number(cellText) > 0

                // Overlap by 1px to the right to eliminate subpixel gaps
                // between adjacent cells that show the table background.
                implicitWidth: (table.columnWidthProvider(column) || 0) + 1
                implicitHeight: table.cachedRowHeight
                color: isSelected ? Theme.rowSelected : (isStriped ? Theme.rowAlt : Theme.surface)
                border.width: 0
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    visible: cellDelegate.isSelected && cellDelegate.column === 0
                    color: Theme.accent
                    opacity: 0.35
                }
                // Thin themed divider on top and bottom edges of the selected
                // row, giving a subtle highlight without heavy borders.
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    visible: cellDelegate.isSelected
                    color: Theme.accent
                    opacity: 0.35
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    visible: cellDelegate.isSelected
                    color: Theme.accent
                    opacity: 0.35
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    text: cellDelegate.cellText
                    color: cellDelegate.opensSam || cellDelegate.isDerivationLink || cellDelegate.opensDerivationGraph ? Theme.link : Theme.text
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: table.cachedTextSize
                    font.bold: cellDelegate.opensSam || cellDelegate.isDerivationLink || cellDelegate.opensDerivationGraph
                    font.underline: cellDelegate.opensSam || cellDelegate.isDerivationLink || cellDelegate.opensDerivationGraph
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: cellDelegate.opensSam || cellDelegate.isDerivationLink || cellDelegate.opensDerivationGraph ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: function (mouse) {
                        root.viewModel.selectRow(cellDelegate.row);
                        if (mouse.button === Qt.RightButton) {
                            cellContextMenu.popup();
                            return;
                        }
                        if (cellDelegate.opensSam) {
                            root.openRequested();
                        } else if (cellDelegate.isDerivationLink) {
                            root.navigateToRelationRequested(cellDelegate.cellText);
                        } else if (cellDelegate.opensDerivationGraph) {
                            root.detailsWindowRequested();
                        }
                    }
                    onDoubleClicked: {
                        root.viewModel.selectRow(cellDelegate.row);
                        root.detailsWindowRequested();
                    }
                }

                Menu {
                    id: cellContextMenu

                    MenuItem {
                        text: "Copiar celula"
                        onTriggered: root.copyTextRequested(cellDelegate.cellText)
                    }
                    MenuItem {
                        text: "Copiar numero SSA"
                        enabled: cellDelegate.rowSsaNumber.length > 0
                        onTriggered: root.copyTextRequested(cellDelegate.rowSsaNumber)
                    }
                    MenuItem {
                        text: "Copiar diagrama SVG"
                        enabled: root.viewModel.details.graphModel.nodeCount > 0
                        onTriggered: root.copyDerivationSvgRequested()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Abrir SAM"
                        enabled: cellDelegate.rowSsaNumber.length > 0
                        onTriggered: root.openRequested()
                    }
                    MenuItem {
                        text: "Abrir tela de detalhes"
                        onTriggered: root.detailsWindowRequested()
                    }
                    MenuItem {
                        text: "Alterar colunas visiveis"
                        onTriggered: root.configureColumnsRequested()
                    }
                }
            }
        }
    }
}
