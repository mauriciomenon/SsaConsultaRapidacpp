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
    property bool showSeriesTags: true
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

    function categoryLabelStride(maxLabelWidth) {
        const targetWidth = Math.min(260, Math.max(70, maxLabelWidth + 12));
        const visibleLabels = Math.max(1, Math.floor(root.plotWidth / targetWidth));
        return Math.max(1, Math.ceil(root.categoryCount / visibleLabels));
    }

    function categoryLabelIndices(stride) {
        const indices = [];
        const firstIndex = stride > 1 ? Math.floor(stride / 2) : 0;
        for (let index = firstIndex; index < root.categoryCount; index += stride)
            indices.push(index);
        return indices;
    }

    function elidedCategoryLabel(context, label, maxWidth) {
        if (context.measureText(label).width <= maxWidth)
            return label;
        let shortened = label;
        while (shortened.length > 1 && context.measureText(shortened + "...").width > maxWidth)
            shortened = shortened.slice(0, -1);
        return shortened + "...";
    }

    function categoryLabelLines(label) {
        return String(label).split("\n");
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

    function seriesTag(seriesIndex) {
        // domain::chartSeriesTag is the single source of truth for tags and the card
        // normalizes every series entry, so an absent tag means "no tag to draw".
        const entry = seriesAt(seriesIndex);
        if (!entry || entry.tag === undefined)
            return "";
        return String(entry.tag);
    }

    function tagTextFor(seriesIndex) {
        return seriesTag(seriesIndex);
    }

    function shouldDrawSeriesTag(segmentHeight, value, seriesIndex) {
        if (!root.showSeriesTags || root.seriesCount < 2)
            return false;
        const tag = seriesTag(seriesIndex);
        if (tag.length === 0)
            return false;
        return segmentHeight >= 10 || value > 0;
    }

    function drawTagBadge(context, centerX, baselineY, text, seriesIndex, insideSegment) {
        context.font = canvasFont("", Theme.fontSizeCaption - 1);
        context.textAlign = "center";
        context.textBaseline = insideSegment ? "bottom" : "top";
        const metrics = context.measureText(text);
        const padX = 3;
        const padY = 1;
        const boxWidth = metrics.width + padX * 2;
        const boxHeight = Theme.fontSizeCaption + 1 + padY * 2;
        const boxX = centerX - boxWidth / 2;
        const boxY = insideSegment ? baselineY - boxHeight : baselineY;
        context.save();
        context.globalAlpha = 0.92;
        context.fillStyle = Theme.panelRaised;
        context.fillRect(boxX, boxY, boxWidth, boxHeight);
        context.globalAlpha = 1;
        context.fillStyle = Theme.readableText(Theme.panelRaised, seriesColor(seriesIndex));
        context.fillText(text, centerX, insideSegment ? baselineY - padY : baselineY + padY);
        context.restore();
    }

    // Canvas font shorthand needs the family quoted, otherwise Qt collapses
    // "Segoe UI" into the unknown family "SegoeUI" and warns on every repaint.
    function canvasFont(weight, pixelSize) {
        const prefix = weight.length === 0 ? "" : weight + " ";
        return prefix + pixelSize + "px \"" + Theme.fontFamily + "\"";
    }

    function drawValueBadge(context, centerX, topY, text) {
        context.font = canvasFont("600", Theme.fontSizeCaption + 1);
        context.textAlign = "center";
        context.textBaseline = "top";
        const metrics = context.measureText(text);
        const padX = 4;
        const padY = 2;
        const boxWidth = metrics.width + padX * 2;
        const boxHeight = Theme.fontSizeCaption + 1 + padY * 2;
        const boxX = centerX - boxWidth / 2;
        const boxY = Math.max(root.topMargin, topY);
        context.save();
        context.globalAlpha = 0.94;
        context.fillStyle = Theme.panelRaised;
        context.fillRect(boxX, boxY, boxWidth, boxHeight);
        context.globalAlpha = 1;
        context.fillStyle = Theme.readableText(Theme.panelRaised, Theme.text);
        context.fillText(text, centerX, boxY + padY);
        context.restore();
    }

    function barLabelMetrics() {
        const tagFontSize = Theme.fontSizeCaption - 1;
        const valueFontSize = Theme.fontSizeCaption + 1;
        return {
            "valueBoxHeight": valueFontSize + 6,
            "tagBoxHeight": tagFontSize + 4,
            "labelGap": 2,
            "edgePad": 2
        };
    }

    function resolveBarSegmentLabelLayout(segmentHeight, wantsValue, wantsTag) {
        const metrics = barLabelMetrics();
        const minInsideBoth = metrics.valueBoxHeight + metrics.tagBoxHeight + metrics.labelGap + metrics.edgePad * 2;
        const minTagInside = metrics.tagBoxHeight + metrics.edgePad * 2;
        const minValueInside = metrics.valueBoxHeight + metrics.edgePad * 2;
        let valuePlacement = "none";
        let tagPlacement = "none";
        if (wantsValue && wantsTag) {
            if (segmentHeight >= minInsideBoth) {
                valuePlacement = "inside";
                tagPlacement = "inside";
            } else {
                valuePlacement = "above";
                tagPlacement = segmentHeight >= minTagInside ? "inside" : "below";
            }
        } else if (wantsValue) {
            valuePlacement = segmentHeight >= minValueInside ? "inside" : "above";
        } else if (wantsTag) {
            tagPlacement = segmentHeight >= minTagInside ? "inside" : "below";
        }
        return {
            "valuePlacement": valuePlacement,
            "tagPlacement": tagPlacement,
            "valueBoxHeight": metrics.valueBoxHeight,
            "tagBoxHeight": metrics.tagBoxHeight
        };
    }

    function drawBarSegmentLabels(context, centerX, y, bottom, seriesIndex, value, valueLimit) {
        const segmentHeight = Math.max(0, bottom - y);
        const valueText = formattedValue(value);
        const wantsValue = root.showValueLabels && valueLimit;
        const wantsTag = shouldDrawSeriesTag(segmentHeight, value, seriesIndex);
        if (!wantsValue && !wantsTag)
            return;
        const layout = resolveBarSegmentLabelLayout(segmentHeight, wantsValue, wantsTag);
        if (layout.valuePlacement === "inside")
            drawValueBadge(context, centerX, y + 2, valueText);
        else if (layout.valuePlacement === "above")
            drawValueBadge(context, centerX, Math.max(root.topMargin, y - layout.valueBoxHeight - 4), valueText);
        if (layout.tagPlacement === "inside")
            drawTagBadge(context, centerX, bottom - 2, seriesTag(seriesIndex), seriesIndex, true);
        else if (layout.tagPlacement === "below")
            drawTagBadge(context, centerX, bottom + 2, seriesTag(seriesIndex), seriesIndex, false);
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

    function grabImage(callback) {
        chartCanvas.requestPaint();
        return chartCanvas.grabToImage(callback);
    }

    function savePlotPng(localPath) {
        if (localPath.length === 0 || !root.hasData) {
            root.plotExportFinished(false);
            return;
        }
        const grabbed = root.grabImage(function(result) {
            root.plotExportFinished(!result.image.isNull() && result.saveToFile(localPath));
        });
        if (!grabbed)
            root.plotExportFinished(false);
    }

    function savePlotSvg(localPath, fileWriter) {
        if (localPath.length === 0 || !root.hasData) {
            root.plotExportFinished(false);
            return;
        }
        const grabbed = root.grabImage(function(result) {
            if (result.image.isNull()) {
                root.plotExportFinished(false);
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
            const wrote = typeof fileWriter === "function" ? fileWriter(localPath, svg) : false;
            root.plotExportFinished(wrote);
        });
        if (!grabbed)
            root.plotExportFinished(false);
    }

    signal plotExportFinished(bool success)

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
        context.font = canvasFont("", Theme.fontSizeCaption);
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

        let maxLabelWidth = 0;
        for (let categoryIndex = 0; categoryIndex < root.categoryCount; ++categoryIndex) {
            const label = displayCategory(categoryIndex, qsTr("Nao atribuido"));
            const lines = categoryLabelLines(label);
            for (let lineIndex = 0; lineIndex < lines.length; ++lineIndex)
                maxLabelWidth = Math.max(maxLabelWidth, context.measureText(lines[lineIndex]).width);
        }
        const labelStride = categoryLabelStride(maxLabelWidth);
        const labelWidth = Math.min(260, Math.max(30, root.plotWidth / Math.max(1, root.categoryCount) * labelStride - 12));
        context.textAlign = "center";
        context.textBaseline = "top";
        const labelIndices = categoryLabelIndices(labelStride);
        for (let index = 0; index < labelIndices.length; ++index) {
            const categoryIndex = labelIndices[index];
            const label = displayCategory(categoryIndex, qsTr("Nao atribuido"));
            const firstLabel = categoryIndex === 0;
            const lastLabel = categoryIndex === root.categoryCount - 1;
            context.textAlign = firstLabel ? "left" : lastLabel ? "right" : "center";
            const labelX = firstLabel ? root.leftMargin : lastLabel ? root.leftMargin + root.plotWidth : xCenter(categoryIndex);
            const lines = categoryLabelLines(label);
            for (let lineIndex = 0; lineIndex < lines.length; ++lineIndex)
                context.fillText(elidedCategoryLabel(context, lines[lineIndex], labelWidth), labelX, root.topMargin + root.plotHeight + 8 + lineIndex * (Theme.fontSizeCaption + 2));
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
                drawBarSegmentLabels(context, x + barWidth / 2, y, bottom, seriesIndex, value, root.showValueLabels);
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
                drawBarSegmentLabels(context, x + barWidth / 2, y, bottom, seriesIndex, value, root.showValueLabels);
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
    onShowSeriesTagsChanged: chartCanvas.requestPaint()
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

        width: Math.max(1, root.width)
        height: Math.max(1, root.height)
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
