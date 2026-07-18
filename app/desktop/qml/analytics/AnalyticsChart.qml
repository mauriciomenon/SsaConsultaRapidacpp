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
    property bool showTableToggle: true
    property bool tableVisible: false
    property int axisTickCount: 5

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

    implicitWidth: 520
    implicitHeight: tableVisible ? 560 : 380
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
                color: Theme.dangerStrong
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: Text.Wrap
                Accessible.name: qsTr("Qualidade dos dados: ") + text
            }
        }

        Item {
            id: plotArea

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220

            AnalyticsChartCanvas {
                id: chartCanvas

                anchors.fill: parent
                chartType: root.chartType
                categories: root.categories
                series: root.series
                trendValues: root.trendValues
                xAxisTitle: root.xAxisTitle
                yAxisTitle: root.yAxisTitle
                valueSuffix: root.valueSuffix
                showValueLabels: root.showValueLabels
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
                visible: !root.hasData
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
            entries: root.legendEntries
        }

        Button {
            id: tableButton

            Layout.alignment: Qt.AlignRight
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
                color: Theme.link
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

        AnalyticsChartTable {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            visible: root.tableVisible
            headers: root.tableHeaders
            rows: root.tableRows
            categoryHeader: root.xAxisTitle.length > 0 ? root.xAxisTitle : qsTr("Categoria")
        }
    }
}
