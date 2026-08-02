import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root

    property string chartType: "bar"
    property string title: ""
    property string subtitle: ""
    property string qualityText: ""
    property var categories: []
    property var series: []
    property var trendValues: []
    property string xAxisTitle: ""
    property string yAxisTitle: ""
    property string valueSuffix: ""
    property string emptyCategoryText: qsTr("Nao atribuido")
    property string missingValueText: qsTr("Sem dado")
    property string emptyMessage: qsTr("Sem dados disponiveis para o periodo selecionado")
    property bool showValueLabels: true
    property bool showSeriesTags: true
    property bool showTableToggle: true
    property bool showExportActions: false
    property bool tableVisible: false
    property bool compact: false
    property int axisTickCount: 5
    property var fileWriter: null

    signal exportFinished(bool success)
    signal exportPngRequested()
    signal exportSvgRequested()

    readonly property bool hasData: chartCanvas.hasData
    readonly property real axisMinimum: chartCanvas.axisMinimum
    readonly property real axisMaximum: chartCanvas.axisMaximum
    readonly property real plotWidth: chartCanvas.plotWidth
    readonly property var tableRows: chartCanvas.modelRevision >= 0 ? chartCanvas.tableRows(emptyCategoryText, missingValueText) : []
    readonly property var tableHeaders: chartCanvas.modelRevision >= 0 ? chartCanvas.legendEntries().map(entry => entry.name) : []
    readonly property var legendEntries: chartCanvas.modelRevision >= 0 ? chartCanvas.legendEntries() : []

    property string tooltipText: ""
    property real tooltipX: 0
    property real tooltipY: 0

    function valueAt(seriesIndex, categoryIndex) {
        return chartCanvas.valueAt(seriesIndex, categoryIndex);
    }

    function trendValueAt(seriesIndex, categoryIndex) {
        return chartCanvas.trendValueAt(seriesIndex, categoryIndex);
    }

    function categoryTotal(categoryIndex) {
        return chartCanvas.categoryTotal(categoryIndex);
    }

    function normalizedValue(categoryIndex, seriesIndex) {
        return chartCanvas.normalizedValue(seriesIndex, categoryIndex);
    }

    function refresh() {
        chartCanvas.refresh();
    }

    function tagTextFor(seriesIndex) {
        return chartCanvas.tagTextFor(seriesIndex);
    }

    function localExportPath(fileUrl) {
        if (fileUrl === null || fileUrl === undefined)
            return "";
        const text = String(fileUrl);
        if (!text.startsWith("file:"))
            return text;
        // QML exposes url values as strings, so the local path has to be rebuilt here:
        // file:///C:/dir keeps the drive letter, file://host/share keeps the UNC prefix
        // and percent escapes must be decoded before reaching saveToFile.
        const path = text.startsWith("file:///") ? text.slice(7) : text.slice(5);
        const decoded = decodeURIComponent(path);
        return /^\/[A-Za-z]:/.test(decoded) ? decoded.slice(1) : decoded;
    }

    function savePng(fileUrl) {
        if (root.compact || !root.hasData) {
            root.exportFinished(false);
            return;
        }
        const localPath = root.localExportPath(fileUrl);
        if (localPath.length === 0) {
            root.exportFinished(false);
            return;
        }
        const grabbed = root.grabToImage(result => {
            root.exportFinished(!result.image.isNull() && result.saveToFile(localPath));
        });
        if (!grabbed)
            root.exportFinished(false);
    }

    function saveSvg(fileUrl) {
        if (root.compact || !root.hasData) {
            root.exportFinished(false);
            return;
        }
        const localPath = root.localExportPath(fileUrl);
        if (localPath.length === 0) {
            root.exportFinished(false);
            return;
        }
        const grabbed = root.grabToImage(result => {
            if (result.image.isNull()) {
                root.exportFinished(false);
                return;
            }
            const buffer = result.image.toDataURL("image/png");
            const comma = buffer.indexOf(",");
            const payload = comma >= 0 ? buffer.slice(comma + 1) : buffer;
            const svg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" +
                        "<svg xmlns=\"http://www.w3.org/2000/svg\" " +
                        "xmlns:xlink=\"http://www.w3.org/1999/xlink\" " +
                        "width=\"" + result.image.width + "\" height=\"" + result.image.height + "\">" +
                        "<image width=\"" + result.image.width + "\" height=\"" + result.image.height + "\" " +
                        "xlink:href=\"data:image/png;base64," + payload + "\"/></svg>";
            const wrote = typeof root.fileWriter === "function" ? root.fileWriter(localPath, svg) : false;
            root.exportFinished(wrote);
        });
        if (!grabbed)
            root.exportFinished(false);
    }

    implicitWidth: 520
    implicitHeight: compact ? 140 : (tableVisible ? 560 : 380)
    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radiusSoft
    Accessible.role: Accessible.Pane
    Accessible.name: title.length > 0 ? title : qsTr("Grafico de analise")
    Accessible.description: [subtitle, qualityText].filter(text => text.length > 0).join(". ")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.cardGap
        spacing: Theme.gap

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            visible: root.title.length > 0 || root.subtitle.length > 0 || root.qualityText.length > 0

            Text {
                Layout.fillWidth: true
                visible: root.title.length > 0
                text: root.title
                textFormat: Text.PlainText
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.subtitle.length > 0
                text: root.subtitle
                textFormat: Text.PlainText
                color: Theme.mutedText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.qualityText.length > 0
                text: root.qualityText
                textFormat: Text.PlainText
                color: Theme.readableText(Theme.panel, Theme.dangerStrong)
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: Text.Wrap
                Accessible.name: qsTr("Qualidade dos dados: ") + text
            }
        }

        Item {
            id: plotArea

            Layout.fillWidth: true
            Layout.fillHeight: !root.compact
            Layout.minimumHeight: root.compact ? 42 : 220
            Layout.preferredHeight: root.compact ? 42 : -1

            AnalyticsChartCanvas {
                id: chartCanvas

                anchors.fill: parent
                visible: !root.compact
                chartType: root.chartType
                categories: root.categories
                series: root.series
                trendValues: root.trendValues
                xAxisTitle: root.xAxisTitle
                yAxisTitle: root.yAxisTitle
                valueSuffix: root.valueSuffix
                showValueLabels: root.showValueLabels
                showSeriesTags: root.showSeriesTags
                tickCount: root.axisTickCount

                onHovered: (categoryIndex, seriesIndex, pointerX, pointerY) => {
                    root.tooltipText = chartCanvas.tooltipText(categoryIndex, seriesIndex, root.emptyCategoryText, root.missingValueText);
                    root.tooltipX = pointerX;
                    root.tooltipY = pointerY;
                }
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, parent.width - Theme.cardGap * 2)
                visible: root.compact || !root.hasData
                text: root.emptyMessage
                textFormat: Text.PlainText
                color: Theme.mutedText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                Accessible.name: text
            }

            AnalyticsChartTooltip {
                text: root.tooltipText
                pointerX: root.tooltipX
                pointerY: root.tooltipY
            }
        }

        AnalyticsChartLegend {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            visible: !root.compact
            entries: root.legendEntries
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !root.compact && ((root.showExportActions && root.hasData) ||
                                       (root.showTableToggle && root.tableRows.length > 0))
            spacing: Theme.gap

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: exportPngButton

                objectName: "analyticsChartExportPng"
                visible: root.showExportActions && root.hasData
                text: qsTr("Exportar PNG")
                flat: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                Accessible.name: text
                onClicked: root.exportPngRequested()

                contentItem: Text {
                    text: exportPngButton.text
                    textFormat: Text.PlainText
                    color: Theme.readableText(exportPngButton.hovered ? Theme.surface : Theme.panel, Theme.link)
                    font: exportPngButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: exportPngButton.hovered ? Theme.surface : "transparent"
                    border.color: exportPngButton.activeFocus ? Theme.accent : "transparent"
                    radius: Theme.radius
                }
            }

            Button {
                id: exportSvgButton

                objectName: "analyticsChartExportSvg"
                visible: root.showExportActions && root.hasData
                text: qsTr("Exportar SVG")
                flat: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                Accessible.name: text
                Accessible.description: ToolTip.text
                ToolTip.visible: hovered
                ToolTip.text: qsTr("SVG com a imagem do grafico embutida (raster, nao vetorial)")
                onClicked: root.exportSvgRequested()

                contentItem: Text {
                    text: exportSvgButton.text
                    textFormat: Text.PlainText
                    color: Theme.readableText(exportSvgButton.hovered ? Theme.surface : Theme.panel, Theme.link)
                    font: exportSvgButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: exportSvgButton.hovered ? Theme.surface : "transparent"
                    border.color: exportSvgButton.activeFocus ? Theme.accent : "transparent"
                    radius: Theme.radius
                }
            }

            Button {
                id: tableButton

                visible: root.showTableToggle && root.tableRows.length > 0
                text: root.tableVisible ? qsTr("Ocultar tabela") : qsTr("Mostrar tabela")
                flat: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                Accessible.name: text
                onClicked: root.tableVisible = !root.tableVisible

                contentItem: Text {
                    text: tableButton.text
                    textFormat: Text.PlainText
                    color: Theme.readableText(tableButton.hovered ? Theme.surface : Theme.panel, Theme.link)
                    font: tableButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: tableButton.hovered ? Theme.surface : "transparent"
                    border.color: tableButton.activeFocus ? Theme.accent : "transparent"
                    radius: Theme.radius
                }
            }
        }

        AnalyticsChartTable {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            visible: !root.compact && root.tableVisible
            headers: root.tableHeaders
            rows: root.tableRows
            categoryHeader: root.xAxisTitle.length > 0 ? root.xAxisTitle : qsTr("Categoria")
        }
    }
}
