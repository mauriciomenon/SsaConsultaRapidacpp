import QtQuick
import SsaConsultaRapida

Item {
    id: root

    required property string chartType
    required property var categories
    required property var series
    property var trendValues: []
    property string xAxisTitle: ""
    property string yAxisTitle: ""
    property string valueSuffix: ""
    property bool showValueLabels: true
    property int tickCount: 5
    property int modelRevision: 0

    readonly property int categoryCount: listCount(categories)
    readonly property int seriesCount: listCount(series)
    readonly property bool hasData: modelRevision >= 0 && computeHasData()
    readonly property real axisMinimum: modelRevision >= 0 ? calculateAxisMinimum() : 0
    readonly property real axisMaximum: modelRevision >= 0 ? calculateAxisMaximum() : 0
    readonly property real plotWidth: Math.max(0, width - leftMargin - rightMargin)
    readonly property real plotHeight: Math.max(0, height - topMargin - bottomMargin)
    readonly property real leftMargin: yAxisTitle.length > 0 ? 62 : 50
    readonly property real rightMargin: 16
    readonly property real topMargin: 18
    readonly property real bottomMargin: xAxisTitle.length > 0 ? 58 : 46
    readonly property var defaultColors: [Theme.accent, Theme.danger, Theme.accentStrong, Theme.link, Theme.mutedText, Theme.border]
    property var hitRegions: []

    signal hovered(int categoryIndex, int seriesIndex, real pointerX, real pointerY)

    function listCount(values) {
        if (values === null || values === undefined)
            return 0;
        if (typeof values.count === "number")
            return values.count;
        return typeof values.length === "number" ? values.length : 0;
    }

    function itemAt(values, index) {
        if (values === null || values === undefined || index < 0 || index >= listCount(values))
            return undefined;
        if (typeof values.get === "function")
            return values.get(index);
        return values[index];
    }

    function seriesAt(index) {
        return itemAt(root.series, index);
    }

    function categoryAt(index) {
        return itemAt(root.categories, index);
    }

    function displayCategory(index, emptyText) {
        const value = categoryAt(index);
        if (value === null || value === undefined || String(value).trim().length === 0)
            return emptyText;
        return String(value);
    }

    function numericValue(value) {
        return typeof value === "number" && Number.isFinite(value) ? value : null;
    }

    function valueAt(seriesIndex, categoryIndex) {
        const entry = seriesAt(seriesIndex);
        if (!entry || entry.values === undefined)
            return null;
        return numericValue(itemAt(entry.values, categoryIndex));
    }

    function trendValueAt(seriesIndex, categoryIndex) {
        const entry = seriesAt(seriesIndex);
        if (entry && entry.trendValues !== undefined)
            return numericValue(itemAt(entry.trendValues, categoryIndex));
        if (root.seriesCount === 1 && listCount(root.trendValues) > 0 && !Array.isArray(itemAt(root.trendValues, 0)))
            return numericValue(itemAt(root.trendValues, categoryIndex));
        const values = itemAt(root.trendValues, seriesIndex);
        return numericValue(itemAt(values, categoryIndex));
    }

    function categoryTotal(categoryIndex) {
        let total = 0;
        for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
            const value = valueAt(seriesIndex, categoryIndex);
            if (value !== null && value >= 0)
                total += value;
        }
        return total;
    }

    function normalizedValue(seriesIndex, categoryIndex) {
        return valueAt(seriesIndex, categoryIndex);
    }

    function computeHasData() {
        for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
            for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
                if (valueAt(seriesIndex, categoryIndex) !== null)
                    return true;
            }
        }
        return false;
    }

    function lineRange() {
        let minimum = null;
        let maximum = null;
        for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
            for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
                const value = valueAt(seriesIndex, categoryIndex);
                const trend = trendValueAt(seriesIndex, categoryIndex);
                if (value !== null) {
                    minimum = minimum === null ? value : Math.min(minimum, value);
                    maximum = maximum === null ? value : Math.max(maximum, value);
                }
                if (trend !== null) {
                    minimum = minimum === null ? trend : Math.min(minimum, trend);
                    maximum = maximum === null ? trend : Math.max(maximum, trend);
                }
            }
        }
        if (minimum === null)
            return {
                "minimum": 0,
                "maximum": 0
            };
        if (minimum === maximum) {
            const padding = Math.max(1, Math.abs(minimum) * 0.1);
            return {
                "minimum": minimum - padding,
                "maximum": maximum + padding
            };
        }
        return {
            "minimum": minimum,
            "maximum": maximum
        };
    }

    function calculateAxisMinimum() {
        return root.chartType === "trendLine" ? lineRange().minimum : 0;
    }

    function calculateAxisMaximum() {
        if (root.chartType === "percentStackedBar")
            return 100;
        if (root.chartType === "trendLine")
            return lineRange().maximum;
        let maximum = 0;
        for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
            if (root.chartType === "stackedBar") {
                maximum = Math.max(maximum, categoryTotal(categoryIndex));
                continue;
            }
            for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
                const value = valueAt(seriesIndex, categoryIndex);
                if (value !== null && value >= 0)
                    maximum = Math.max(maximum, value);
            }
        }
        return maximum;
    }

    function formatNumber(value) {
        if (value === null || !Number.isFinite(value))
            return "";
        const rounded = Math.round(value * 10) / 10;
        return Number.isInteger(rounded) ? String(rounded) : rounded.toFixed(1);
    }

    function formattedValue(value) {
        const text = formatNumber(value);
        return text.length > 0 ? text + root.valueSuffix : text;
    }

    function seriesName(seriesIndex) {
        const entry = seriesAt(seriesIndex);
        if (entry && entry.name !== undefined && String(entry.name).trim().length > 0)
            return String(entry.name);
        return qsTr("Serie %1").arg(seriesIndex + 1);
    }

    function seriesColor(seriesIndex) {
        const entry = seriesAt(seriesIndex);
        if (entry && entry.color !== undefined && String(entry.color).length > 0)
            return entry.color;
        return root.defaultColors[seriesIndex % root.defaultColors.length];
    }

    function legendEntries() {
        const entries = [];
        for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
            entries.push({
                "name": seriesName(seriesIndex),
                "color": seriesColor(seriesIndex),
                "hasTrend": hasTrend(seriesIndex)
            });
        }
        return entries;
    }

    function hasTrend(seriesIndex) {
        for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
            if (trendValueAt(seriesIndex, categoryIndex) !== null)
                return true;
        }
        return false;
    }

    function tableRows(emptyCategoryText, missingValueText) {
        const rows = [];
        for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
            const values = [];
            for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
                const value = valueAt(seriesIndex, categoryIndex);
                values.push(value === null ? missingValueText : formattedValue(value));
            }
            rows.push({
                "category": displayCategory(categoryIndex, emptyCategoryText),
                "values": values
            });
        }
        return rows;
    }

    function tooltipText(categoryIndex, seriesIndex, emptyCategoryText, missingValueText) {
        if (categoryIndex < 0 || categoryIndex >= root.categoryCount)
            return "";
        const lines = [displayCategory(categoryIndex, emptyCategoryText)];
        if (seriesIndex >= 0) {
            const value = valueAt(seriesIndex, categoryIndex);
            lines.push(seriesName(seriesIndex) + ": " + (value === null ? missingValueText : formattedValue(value)));
        } else {
            for (let index = 0; index < root.seriesCount; ++index) {
                const value = valueAt(index, categoryIndex);
                lines.push(seriesName(index) + ": " + (value === null ? missingValueText : formattedValue(value)));
            }
        }
        return lines.join("\n");
    }

    function refresh() {
        ++root.modelRevision;
        chartCanvas.requestPaint();
    }

    function yFor(value, minimum, maximum) {
        const range = Math.max(0.000001, maximum - minimum);
        return root.topMargin + (maximum - value) * root.plotHeight / range;
    }

    function xCenter(categoryIndex) {
        if (root.categoryCount === 0)
            return root.leftMargin;
        return root.leftMargin + (categoryIndex + 0.5) * root.plotWidth / root.categoryCount;
    }

    function hitAt(pointerX, pointerY) {
        for (let index = root.hitRegions.length - 1; index >= 0; --index) {
            const region = root.hitRegions[index];
            if (pointerX >= region.x && pointerX <= region.x + region.width && pointerY >= region.y && pointerY <= region.y + region.height)
                return region;
        }
        return null;
    }

    function drawAxes(context, minimum, maximum) {
        context.strokeStyle = Theme.border;
        context.fillStyle = Theme.mutedText;
        context.lineWidth = 1;
        context.font = Theme.fontSizeCaption + "px " + Theme.fontFamily;
        context.textBaseline = "middle";
        const ticks = Math.max(2, root.tickCount);
        for (let tick = 0; tick <= ticks; ++tick) {
            const ratio = tick / ticks;
            const value = maximum - ratio * (maximum - minimum);
            const y = root.topMargin + ratio * root.plotHeight;
            context.beginPath();
            context.moveTo(root.leftMargin, y);
            context.lineTo(root.width - root.rightMargin, y);
            context.stroke();
            context.textAlign = "right";
            context.fillText(formatNumber(value) + root.valueSuffix, root.leftMargin - 7, y);
        }

        const labelStride = Math.max(1, Math.ceil(root.categoryCount / Math.max(1, Math.floor(root.plotWidth / 70))));
        context.textAlign = "center";
        context.textBaseline = "top";
        for (let categoryIndex = 0; categoryIndex < root.categoryCount; categoryIndex += labelStride) {
            const label = displayCategory(categoryIndex, qsTr("Nao atribuido"));
            context.fillText(label, xCenter(categoryIndex), root.topMargin + root.plotHeight + 8, Math.max(30, root.plotWidth / Math.max(1, root.categoryCount) * labelStride - 4));
        }
        if ((root.categoryCount - 1) % labelStride !== 0 && root.categoryCount > 1) {
            const lastIndex = root.categoryCount - 1;
            context.fillText(displayCategory(lastIndex, qsTr("Nao atribuido")), xCenter(lastIndex), root.topMargin + root.plotHeight + 8, Math.max(30, root.plotWidth / Math.max(1, root.categoryCount) * labelStride - 4));
        }

        if (root.xAxisTitle.length > 0) {
            context.fillStyle = Theme.text;
            context.textAlign = "center";
            context.textBaseline = "bottom";
            context.fillText(root.xAxisTitle, root.leftMargin + root.plotWidth / 2, root.height - 2);
        }
        if (root.yAxisTitle.length > 0) {
            context.save();
            context.fillStyle = Theme.text;
            context.textAlign = "center";
            context.textBaseline = "top";
            context.translate(5, root.topMargin + root.plotHeight / 2);
            context.rotate(-Math.PI / 2);
            context.fillText(root.yAxisTitle, 0, 0);
            context.restore();
        }
    }

    function drawSimpleBars(context, minimum, maximum, regions) {
        if (root.categoryCount === 0 || root.seriesCount === 0)
            return;
        const slotWidth = root.plotWidth / root.categoryCount;
        const groupWidth = slotWidth * 0.72;
        const barWidth = Math.max(1, groupWidth / root.seriesCount);
        for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
            const groupX = root.leftMargin + categoryIndex * slotWidth + (slotWidth - groupWidth) / 2;
            for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
                const value = valueAt(seriesIndex, categoryIndex);
                if (value === null || value < 0)
                    continue;
                const x = groupX + seriesIndex * barWidth;
                const y = yFor(value, minimum, maximum);
                const bottom = yFor(0, minimum, maximum);
                context.fillStyle = seriesColor(seriesIndex);
                context.fillRect(x + 1, y, Math.max(1, barWidth - 2), Math.max(0, bottom - y));
                regions.push({
                    "x": x,
                    "y": y,
                    "width": barWidth,
                    "height": Math.max(3, bottom - y),
                    "categoryIndex": categoryIndex,
                    "seriesIndex": seriesIndex
                });
                if (root.showValueLabels && root.categoryCount <= 24) {
                    context.fillStyle = Theme.text;
                    context.textAlign = "center";
                    context.textBaseline = "bottom";
                    context.fillText(formattedValue(value), x + barWidth / 2, Math.max(root.topMargin, y - 3));
                }
            }
        }
    }

    function drawStackedBars(context, minimum, maximum, regions) {
        if (root.categoryCount === 0 || root.seriesCount === 0)
            return;
        const slotWidth = root.plotWidth / root.categoryCount;
        const barWidth = Math.max(2, slotWidth * 0.62);
        for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
            let runningValue = 0;
            const x = root.leftMargin + categoryIndex * slotWidth + (slotWidth - barWidth) / 2;
            for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
                const value = valueAt(seriesIndex, categoryIndex);
                if (value === null || value < 0)
                    continue;
                const y = yFor(runningValue + value, minimum, maximum);
                const bottom = yFor(runningValue, minimum, maximum);
                context.fillStyle = seriesColor(seriesIndex);
                context.fillRect(x, y, barWidth, Math.max(0, bottom - y));
                regions.push({
                    "x": x,
                    "y": y,
                    "width": barWidth,
                    "height": Math.max(3, bottom - y),
                    "categoryIndex": categoryIndex,
                    "seriesIndex": seriesIndex
                });
                if (root.showValueLabels && root.categoryCount <= 18 && bottom - y >= 16) {
                    context.fillStyle = Theme.readableText(seriesColor(seriesIndex));
                    context.textAlign = "center";
                    context.textBaseline = "middle";
                    context.fillText(formattedValue(value), x + barWidth / 2, y + (bottom - y) / 2);
                }
                runningValue += value;
            }
        }
    }

    function drawLineValues(context, minimum, maximum, regions) {
        for (let seriesIndex = 0; seriesIndex < root.seriesCount; ++seriesIndex) {
            context.strokeStyle = seriesColor(seriesIndex);
            context.fillStyle = seriesColor(seriesIndex);
            context.lineWidth = 2;
            context.setLineDash([]);
            context.beginPath();
            let segmentStarted = false;
            for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
                const value = valueAt(seriesIndex, categoryIndex);
                if (value === null) {
                    segmentStarted = false;
                    continue;
                }
                const x = xCenter(categoryIndex);
                const y = yFor(value, minimum, maximum);
                if (!segmentStarted) {
                    context.moveTo(x, y);
                    segmentStarted = true;
                } else {
                    context.lineTo(x, y);
                }
            }
            context.stroke();

            for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
                const value = valueAt(seriesIndex, categoryIndex);
                if (value === null)
                    continue;
                const x = xCenter(categoryIndex);
                const y = yFor(value, minimum, maximum);
                context.beginPath();
                context.arc(x, y, 3, 0, Math.PI * 2);
                context.fill();
                regions.push({
                    "x": x - 7,
                    "y": y - 7,
                    "width": 14,
                    "height": 14,
                    "categoryIndex": categoryIndex,
                    "seriesIndex": seriesIndex
                });
                if (root.showValueLabels && root.categoryCount <= 18) {
                    context.fillStyle = Theme.text;
                    context.textAlign = "center";
                    context.textBaseline = "bottom";
                    context.fillText(formattedValue(value), x, y - 6);
                    context.fillStyle = seriesColor(seriesIndex);
                }
            }

            if (!hasTrend(seriesIndex))
                continue;
            context.strokeStyle = seriesColor(seriesIndex);
            context.lineWidth = 1.5;
            context.setLineDash([5, 4]);
            context.beginPath();
            let trendStarted = false;
            for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
                const trend = trendValueAt(seriesIndex, categoryIndex);
                if (trend === null) {
                    trendStarted = false;
                    continue;
                }
                const x = xCenter(categoryIndex);
                const y = yFor(trend, minimum, maximum);
                if (!trendStarted) {
                    context.moveTo(x, y);
                    trendStarted = true;
                } else {
                    context.lineTo(x, y);
                }
            }
            context.stroke();
            context.setLineDash([]);
        }
    }

    function paintChart(context) {
        context.reset();
        context.clearRect(0, 0, root.width, root.height);
        const minimum = root.axisMinimum;
        const maximum = root.axisMaximum > minimum ? root.axisMaximum : minimum + 1;
        drawAxes(context, minimum, maximum);
        const regions = [];
        if (!root.hasData) {
            root.hitRegions = regions;
            return;
        }
        if (root.chartType === "bar")
            drawSimpleBars(context, minimum, maximum, regions);
        else if (root.chartType === "stackedBar")
            drawStackedBars(context, minimum, maximum, regions);
        else if (root.chartType === "percentStackedBar")
            drawStackedBars(context, minimum, maximum, regions);
        else if (root.chartType === "trendLine")
            drawLineValues(context, minimum, maximum, regions);
        root.hitRegions = regions;
    }

    onCategoriesChanged: chartCanvas.requestPaint()
    onSeriesChanged: chartCanvas.requestPaint()
    onTrendValuesChanged: chartCanvas.requestPaint()
    onChartTypeChanged: chartCanvas.requestPaint()
    onTickCountChanged: chartCanvas.requestPaint()
    onShowValueLabelsChanged: chartCanvas.requestPaint()
    onValueSuffixChanged: chartCanvas.requestPaint()
    onXAxisTitleChanged: chartCanvas.requestPaint()
    onYAxisTitleChanged: chartCanvas.requestPaint()
    onWidthChanged: chartCanvas.requestPaint()
    onHeightChanged: chartCanvas.requestPaint()

    Connections {
        target: Theme

        function onThemeNameChanged() {
            chartCanvas.requestPaint();
        }
    }

    Canvas {
        id: chartCanvas

        anchors.fill: parent
        renderTarget: Canvas.Image

        onPaint: root.paintChart(getContext("2d"))
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onExited: root.hovered(-1, -1, 0, 0)
        onPositionChanged: mouse => {
            const region = root.hitAt(mouse.x, mouse.y);
            if (region) {
                root.hovered(region.categoryIndex, region.seriesIndex, mouse.x, mouse.y);
                return;
            }
            root.hovered(-1, -1, mouse.x, mouse.y);
        }
    }
}
