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
    signal configureColumnsRequested(var trigger)
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
    property int draggedColumnIndex: -1
    property string contextCellText: ""
    property string contextRowText: ""
    property string contextSsaNumber: ""

    function firstCellCenterForSmoke() {
        const cell = table.itemAtCell(Qt.point(0, 0));
        if (cell === null)
            return Qt.point(-1, -1);
        return cell.mapToItem(root, cell.width / 2, cell.height / 2);
    }

    function headerCellForSmoke(columnKey) {
        for (let index = 0; index < headerRepeater.count; ++index) {
            const cell = headerRepeater.itemAt(index);
            if (cell !== null && cell.objectName === "ssaHeaderCell_" + columnKey)
                return cell;
        }
        return null;
    }

    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius
    clip: true
    activeFocusOnTab: true
    Accessible.role: Accessible.Table
    Accessible.name: "Tabela de SSAs"

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Down) {
            if (root.viewModel.currentRow < 0 && root.viewModel.totalRows > 0)
                root.viewModel.selectRow(0);
            else if (root.viewModel.canSelectNextRow)
                root.viewModel.selectNextRow();
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            if (root.viewModel.canSelectPreviousRow)
                root.viewModel.selectPreviousRow();
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (root.viewModel.currentRow >= 0)
                root.detailsWindowRequested();
            event.accepted = true;
        }
    }

    Menu {
        id: cellContextMenu
        objectName: "cellContextMenu"
        parent: root

        MenuItem {
            objectName: "copyCellAction"
            property string actionId: "copy_cell"
            text: "Copiar celula"
            enabled: root.contextCellText.length > 0
            onTriggered: root.copyTextRequested(root.contextCellText)
        }
        MenuItem {
            objectName: "copyRowAction"
            property string actionId: "copy_row"
            text: "Copiar linha"
            enabled: root.contextRowText.length > 0
            onTriggered: root.copyTextRequested(root.contextRowText)
        }
        MenuItem {
            objectName: "copySsaAction"
            property string actionId: "copy_ssa"
            text: "Copiar numero SSA"
            enabled: root.contextSsaNumber.length > 0
            onTriggered: root.copyTextRequested(root.contextSsaNumber)
        }
        MenuItem {
            objectName: "copyGraphAction"
            property string actionId: "copy_graph_svg"
            text: "Copiar diagrama SVG"
            enabled: root.viewModel.details.graphModel.nodeCount > 0
            onTriggered: root.copyDerivationSvgRequested()
        }
        MenuSeparator {}
        MenuItem {
            objectName: "openSamAction"
            property string actionId: "open_sam"
            text: "Abrir SAM"
            enabled: root.contextSsaNumber.length > 0
            onTriggered: root.openRequested()
        }
        MenuItem {
            objectName: "openDetailsAction"
            property string actionId: "open_details"
            text: "Abrir tela de detalhes"
            enabled: root.contextSsaNumber.length > 0
            onTriggered: root.detailsWindowRequested()
        }
        MenuItem {
            id: cellConfigureColumnsAction
            objectName: "cellConfigureColumnsAction"
            property string actionId: "configure_columns_from_cell"
            text: "Alterar colunas visiveis"
            onTriggered: root.configureColumnsRequested(cellConfigureColumnsAction)
        }
    }

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
                    id: headerRepeater
                    model: root.tableColumns

                    delegate: Rectangle {
                        id: headerCell
                        objectName: "ssaHeaderCell_" + columnKey
                        property alias contextMenuForSmoke: headerMenu
                        required property int index
                        required property var modelData
                        readonly property bool hasColumnKey: modelData.key !== undefined && modelData.key !== null && modelData.key !== ""
                        readonly property string columnKey: hasColumnKey ? modelData.key : ""
                        readonly property bool centerText: columnKey === "situacao" || columnKey === "derivada_de" || columnKey === "localizacao_codigo" || columnKey === "setor_emissor" || columnKey === "setor_executor" || columnKey === "qtd_derivadas" || columnKey === "data_cadastro" || columnKey === "semana_cadastro" || columnKey === "semana_programada" || columnKey === "semana_executada"
                        readonly property int modelWidth: modelData.width !== undefined && modelData.width !== null ? modelData.width : root.fallbackColumnWidth
                        readonly property string effectiveLabel: {
                            const shortLabel = modelData.label !== undefined ? modelData.label : "";
                            const full = modelData.labelFull !== undefined ? modelData.labelFull : "";
                            return shortLabel.length > 0 ? shortLabel : full;
                        }
                        property int dragStartWidth: 0
                        property real dragStartX: 0
                        property int previewWidth: modelWidth
                        property bool reorderTarget: false

                        width: previewWidth
                        height: header.height
                        color: reorderTarget ? Theme.accentSoft : Theme.tableHeader
                        border.color: Theme.borderSoft
                        border.width: 0

                        Drag.active: reorderDragArea.pressed && root.draggedColumnIndex === index
                        Drag.source: headerCell
                        Drag.keys: ["ssa-column"]
                        Drag.hotSpot.x: width / 2
                        Drag.hotSpot.y: height / 2

                        onModelDataChanged: previewWidth = modelWidth

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            text: headerCell.effectiveLabel + (headerCell.modelData.sorted ? (headerCell.modelData.sortAscending ? "  ^" : "  v") : "")
                            color: Theme.accentStrong
                            font.bold: true
                            font.pixelSize: root.textSize
                            horizontalAlignment: headerCell.centerText ? Text.AlignHCenter : Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: headerCell.modelData.filtered === true
                            anchors.right: parent.right
                            anchors.rightMargin: 3
                            anchors.verticalCenter: parent.verticalCenter
                            text: "f"
                            color: Theme.accentStrong
                            font.bold: true
                            font.pixelSize: Math.max(9, root.textSize - 3)
                        }

                        ToolTip.visible: headerMouseArea.containsMouse
                        ToolTip.text: headerCell.modelData.labelFull !== undefined && headerCell.modelData.labelFull.length > 0 ? headerCell.modelData.labelFull : headerCell.effectiveLabel
                        ToolTip.delay: 500

                        MouseArea {
                            id: headerMouseArea
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

                        Rectangle {
                            id: reorderHandle
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 14
                            visible: root.columnFlow !== null && headerCell.hasColumnKey
                            color: reorderDragArea.containsMouse || reorderDragArea.pressed ? Theme.accentSoft : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "::"
                                color: Theme.mutedText
                                font.bold: true
                                font.pixelSize: Math.max(9, root.textSize - 3)
                            }

                            MouseArea {
                                id: reorderDragArea
                                anchors.fill: parent
                                cursorShape: Qt.SizeAllCursor
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton
                                onPressed: root.draggedColumnIndex = headerCell.index
                                onReleased: root.draggedColumnIndex = -1
                            }
                        }

                        DropArea {
                            id: headerDropArea
                            anchors.fill: parent
                            z: 2
                            keys: ["ssa-column"]

                            onEntered: function (drag) {
                                headerCell.reorderTarget = drag.source !== headerCell;
                            }
                            onExited: headerCell.reorderTarget = false
                            onDropped: function (drop) {
                                const source = drop.source;
                                if (source !== null && source !== undefined && source !== headerCell && root.draggedColumnIndex >= 0 && root.columnFlow !== null) {
                                    root.columnFlow.moveVisibleColumnAndApply(root.draggedColumnIndex, headerCell.index);
                                }
                                headerCell.reorderTarget = false;
                            }
                        }

                        Menu {
                            id: headerMenu
                            objectName: "headerContextMenu_" + headerCell.columnKey

                            MenuItem {
                                objectName: "filterColumnAction"
                                property string actionId: "filter_column"
                                text: "Filtrar coluna"
                                enabled: headerCell.hasColumnKey
                                onTriggered: root.viewModel.setFilterPanelFocusColumn(headerCell.columnKey)
                            }
                            MenuItem {
                                objectName: "hideColumnAction"
                                property string actionId: "hide_column"
                                text: "Ocultar coluna"
                                enabled: headerCell.hasColumnKey && root.columnFlow !== null && root.columnFlow.canHideColumn(headerCell.columnKey)
                                onTriggered: root.columnFlow.setColumnVisibleAndApply(headerCell.columnKey, false)
                            }
                            MenuItem {
                                objectName: "resetSortAction"
                                property string actionId: "reset_sort"
                                text: "Limpar ordenacao"
                                enabled: root.viewModel.sortColumnKey.length > 0
                                onTriggered: root.viewModel.resetSort()
                            }
                            MenuSeparator {}
                            MenuItem {
                                id: headerConfigureColumnsAction
                                objectName: "headerConfigureColumnsAction"
                                property string actionId: "configure_columns_from_header"
                                text: "Configurar colunas"
                                onTriggered: root.configureColumnsRequested(headerConfigureColumnsAction)
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
                readonly property bool hasCellText: displayValue !== undefined && displayValue !== null && String(displayValue).length > 0
                readonly property string cellText: hasCellText ? String(displayValue) : ""
                readonly property bool isDerivationLink: columnConfig.key === "derivada_de" && hasCellText
                readonly property bool opensDerivationGraph: columnConfig.key === "qtd_derivadas" && Number(cellText) > 0
                readonly property bool centerText: columnConfig.key === "situacao" || columnConfig.key === "derivada_de" || columnConfig.key === "localizacao_codigo" || columnConfig.key === "setor_emissor" || columnConfig.key === "setor_executor" || columnConfig.key === "qtd_derivadas" || columnConfig.key === "data_cadastro" || columnConfig.key === "semana_cadastro" || columnConfig.key === "semana_programada" || columnConfig.key === "semana_executada"

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
                    textFormat: Text.PlainText
                    color: cellDelegate.opensSam || cellDelegate.isDerivationLink || cellDelegate.opensDerivationGraph ? Theme.accentStrong : Theme.text
                    horizontalAlignment: cellDelegate.centerText ? Text.AlignHCenter : Text.AlignLeft
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
                        root.forceActiveFocus();
                        root.viewModel.selectRow(cellDelegate.row);
                        if (mouse.button === Qt.RightButton) {
                            root.contextCellText = cellDelegate.cellText;
                            root.contextRowText = root.viewModel.tableModel.rowText(cellDelegate.row);
                            root.contextSsaNumber = root.viewModel.tableModel.ssaNumberAt(cellDelegate.row);
                            const menuParent = Overlay.overlay !== null ? Overlay.overlay : root;
                            if (cellContextMenu.parent !== menuParent)
                                cellContextMenu.parent = menuParent;
                            const menuPosition = cellDelegate.mapToItem(menuParent, mouse.x, mouse.y);
                            cellContextMenu.x = Theme.clampedPopupX(menuParent.width, menuPosition.x + cellContextMenu.width, cellContextMenu.width);
                            cellContextMenu.y = Theme.clampedPopupY(menuParent.height, menuPosition.y, 0, cellContextMenu.height);
                            cellContextMenu.open();
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
                        if (cellDelegate.opensSam || cellDelegate.isDerivationLink || cellDelegate.opensDerivationGraph)
                            return;
                        root.detailsWindowRequested();
                    }
                }
            }
        }
    }
}
