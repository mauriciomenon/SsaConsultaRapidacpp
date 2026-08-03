import QtQuick
import QtTest

TestCase {
    id: testCase

    name: "AnalyticsCharts"
    when: windowShown

    property var chart: null

    function cleanup() {
        if (chart)
            chart.destroy();
        chart = null;
    }

    function createChart(component) {
        chart = createTemporaryObject(component, testCase, {
            "width": 640,
            "height": 360
        });
        verify(chart !== null);
        waitForRendering(chart);
        return chart;
    }

    Component {
        id: simpleBarComponent

        SimpleBarChart {
            title: "Executadas"
            categories: ["SEE", ""]
            series: [
                {
                    "name": "SSAs",
                    "values": [5, null]
                }
            ]
        }
    }

    Component {
        id: stackedBarComponent

        StackedBarChart {
            categories: ["SEE", "MEL"]
            series: [
                {
                    "name": "Periodo",
                    "values": [3, 1]
                },
                {
                    "name": "Anteriores",
                    "values": [4, 2]
                }
            ]
        }
    }

    Component {
        id: taggedStackedBarComponent

        StackedBarChart {
            categories: ["SEE"]
            series: [
                {
                    "name": "Joao Silva Santos",
                    "tag": "JSS",
                    "values": [1]
                },
                {
                    "name": "Maria",
                    "tag": "M",
                    "values": [0]
                },
                {
                    "name": "IEE2",
                    "tag": "IEE2",
                    "values": [0]
                },
                {
                    "name": "Ana Costa",
                    "tag": "AC",
                    "values": [0]
                }
            ]
        }
    }

    Component {
        id: shortTaggedStackedBarComponent

        StackedBarChart {
            width: 640
            height: 360
            categories: ["Short", "Tall"]
            series: [
                {
                    "name": "Afra Person",
                    "tag": "AFRA",
                    "values": [1, 40]
                },
                {
                    "name": "LCC Person",
                    "tag": "LCC",
                    "values": [1, 60]
                }
            ]
        }
    }

    Component {
        id: shortTaggedGroupedBarComponent

        SimpleBarChart {
            width: 640
            height: 360
            categories: ["Short", "Tall"]
            series: [
                {
                    "name": "Afra Person",
                    "tag": "AFRA",
                    "values": [1, 100]
                },
                {
                    "name": "LCC Person",
                    "tag": "LCC",
                    "values": [1, 80]
                }
            ]
        }
    }

    Component {
        id: percentBarComponent

        PercentStackedBarChart {
            categories: ["2026-W01"]
            series: [
                {
                    "name": "No prazo",
                    "values": [25]
                },
                {
                    "name": "Alerta",
                    "values": [75]
                }
            ]
        }
    }

    Component {
        id: totalOnlyBarComponent

        SimpleBarChart {
            categories: ["Ana", "Bruno"]
            series: [
                {
                    "key": "total",
                    "name": "Total",
                    "tag": "",
                    "values": [1, 2]
                }
            ]
        }
    }

    Component {
        id: trendLineComponent

        TrendLineChart {
            categories: ["Jan", "Fev", "Mar"]
            series: [
                {
                    "name": "Executadas",
                    "values": [10, null, 30],
                    "trendValues": [12, 20, 28]
                }
            ]
        }
    }

    function test_simple_bars_start_at_zero_and_keep_missing_values() {
        const item = createChart(simpleBarComponent);

        compare(item.chartType, "bar");
        verify(item.hasData);
        compare(item.axisMinimum, 0);
        compare(item.axisMaximum, 5);
        compare(item.tableRows.length, 2);
        compare(item.tableRows[0].category, "SEE");
        compare(item.tableRows[0].values[0], "5");
        compare(item.tableRows[1].category, "Nao atribuido");
        compare(item.tableRows[1].values[0], "Sem dado");
    }

    function test_stacked_bars_reconcile_series_totals() {
        const item = createChart(stackedBarComponent);

        compare(item.chartType, "stackedBar");
        compare(item.axisMinimum, 0);
        compare(item.axisMaximum, 7);
        compare(item.categoryTotal(0), 7);
        compare(item.categoryTotal(1), 3);
    }

    function test_stacked_bars_expose_series_tags_for_harness() {
        const item = createChart(taggedStackedBarComponent);

        compare(item.tagTextFor(0), "JSS");
        compare(item.tagTextFor(1), "M");
        compare(item.tagTextFor(2), "IEE2");
        compare(item.tagTextFor(3), "AC");
    }

    function test_short_bar_label_layout_keeps_value_above_tag() {
        const layout = createChart(simpleBarComponent).barSegmentLabelLayout(22, true, true);

        compare(layout.valuePlacement, "above");
        compare(layout.tagPlacement, "inside");
    }

    function test_tall_bar_label_layout_allows_both_inside() {
        const layout = createChart(simpleBarComponent).barSegmentLabelLayout(40, true, true);

        compare(layout.valuePlacement, "inside");
        compare(layout.tagPlacement, "inside");
    }

    function test_very_short_bar_label_layout_puts_tag_below() {
        const layout = createChart(simpleBarComponent).barSegmentLabelLayout(8, true, true);

        compare(layout.valuePlacement, "above");
        compare(layout.tagPlacement, "below");
    }

    function test_short_tagged_stacked_bars_render_with_values_and_tags() {
        const item = createChart(shortTaggedStackedBarComponent);

        compare(item.tagTextFor(0), "AFRA");
        compare(item.tagTextFor(1), "LCC");
        compare(item.showValueLabels, true);
        compare(item.showSeriesTags, true);
        compare(item.axisMaximum, 100);

        const segmentHeight = Math.max(1, Math.round(item.plotHeight / item.axisMaximum));
        const layout = item.barSegmentLabelLayout(segmentHeight, true, true);
        compare(layout.valuePlacement, "above");
        verify(layout.tagPlacement === "inside" || layout.tagPlacement === "below");
    }

    function test_short_tagged_grouped_bars_render_with_values_and_tags() {
        const item = createChart(shortTaggedGroupedBarComponent);

        compare(item.tagTextFor(0), "AFRA");
        compare(item.tagTextFor(1), "LCC");
        compare(item.axisMaximum, 100);

        const segmentHeight = Math.max(1, Math.round(item.plotHeight / item.axisMaximum));
        const layout = item.barSegmentLabelLayout(segmentHeight, true, true);
        compare(layout.valuePlacement, "above");
        verify(layout.tagPlacement === "inside" || layout.tagPlacement === "below");
    }

    function test_percent_bars_use_fixed_scale_and_normalized_values() {
        const item = createChart(percentBarComponent);

        compare(item.chartType, "percentStackedBar");
        compare(item.axisMinimum, 0);
        compare(item.axisMaximum, 100);
        compare(item.normalizedValue(0, 0), 25);
        compare(item.normalizedValue(0, 1), 75);
    }

    function test_single_total_series_hides_tags() {
        const item = createChart(totalOnlyBarComponent);

        compare(item.tagTextFor(0), "");
    }

    function test_chart_exposes_png_and_svg_export_invokables() {
        const item = createChart(simpleBarComponent);

        compare(typeof item.savePng, "function");
        compare(typeof item.saveSvg, "function");
    }

    function test_export_path_decodes_dialog_urls() {
        const item = createChart(simpleBarComponent);

        compare(item.localExportPath("file:///C:/Users/ana/Meus%20Graficos/ssa.png"),
                "C:/Users/ana/Meus Graficos/ssa.png");
        compare(item.localExportPath("file:///home/ana/ssa%20chart.png"),
                "/home/ana/ssa chart.png");
        compare(item.localExportPath(""), "");
        compare(item.ensureExportExtension("C:/tmp/chart", "png"), "C:/tmp/chart.png");
        compare(item.ensureExportExtension("C:/tmp/chart.PNG", "png"), "C:/tmp/chart.PNG");
    }

    function findChildByObjectName(parentItem, objectName) {
        if (parentItem === null || parentItem === undefined)
            return null;
        if (parentItem.objectName === objectName)
            return parentItem;
        const children = parentItem.children;
        for (let index = 0; index < children.length; ++index) {
            const found = findChildByObjectName(children[index], objectName);
            if (found !== null)
                return found;
        }
        return null;
    }

    function test_export_grabs_full_card_snapshot() {
        const item = createChart(shortTaggedStackedBarComponent);
        item.title = "AFRA/LCC Export";

        const snapshot = findChildByObjectName(item, "analyticsChartSnapshot");
        const canvas = findChildByObjectName(item, "analyticsChartCanvas");
        verify(snapshot !== null);
        verify(canvas !== null);

        let grabbedItem = null;
        item.itemGrabber = function(target, path) {
            grabbedItem = target;
            return path.length > 0;
        };
        item.savePng("C:/tmp/analytics-export-test.png");
        compare(grabbedItem, snapshot);
        verify(grabbedItem !== canvas);
    }

    function test_export_snapshot_taller_than_plot_canvas() {
        const item = createChart(shortTaggedStackedBarComponent);
        item.title = "AFRA/LCC Export";

        const snapshot = findChildByObjectName(item, "analyticsChartSnapshot");
        const canvas = findChildByObjectName(item, "analyticsChartCanvas");
        verify(snapshot !== null);
        verify(canvas !== null);
        verify(snapshot.height > canvas.height);

        let snapshotGrab = null;
        let canvasGrab = null;
        snapshot.grabToImage(function(result) {
            snapshotGrab = result;
        });
        canvas.grabToImage(function(result) {
            canvasGrab = result;
        });
        tryVerify(function() {
            return snapshotGrab !== null && canvasGrab !== null
                   && !snapshotGrab.image.isNull() && !canvasGrab.image.isNull();
        });
        verify(snapshotGrab.image.height > canvasGrab.image.height);
    }

    Component {
        id: manyCategoryBarComponent

        SimpleBarChart {
            categories: ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
                         "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
                         "U", "V", "W", "X", "Y", "Z", "AA"]
            series: [
                {
                    "name": "SSAs",
                    "values": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                               11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                               21, 22, 23, 24, 25, 26, 27]
                }
            ]
        }
    }

    function test_many_categories_keep_value_labels_enabled() {
        const item = createChart(manyCategoryBarComponent);

        compare(item.categories.length, 27);
        compare(item.axisMaximum, 27);
        compare(item.showValueLabels, true);
    }

    function test_line_chart_uses_automatic_scale_and_keeps_gap() {
        const item = createChart(trendLineComponent);

        compare(item.chartType, "trendLine");
        compare(item.axisMinimum, 10);
        compare(item.axisMaximum, 30);
        compare(item.valueAt(0, 1), null);
        compare(item.valueAt(0, 2), 30);
        compare(item.trendValueAt(0, 1), 20);
    }

    function test_resize_updates_canvas_without_changing_data_contract() {
        const item = createChart(simpleBarComponent);
        const originalWidth = item.plotWidth;

        item.width = 420;
        waitForRendering(item);

        verify(item.plotWidth < originalWidth);
        compare(item.tableRows[0].values[0], "5");
    }

    function test_refresh_rebuilds_derived_models_after_in_place_update() {
        const item = createChart(simpleBarComponent);

        item.series[0].values[0] = 9;
        item.refresh();

        tryCompare(item, "axisMaximum", 9);
        compare(item.tableRows[0].values[0], "9");
    }
}
