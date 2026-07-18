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

    function test_percent_bars_use_fixed_scale_and_normalized_values() {
        const item = createChart(percentBarComponent);

        compare(item.chartType, "percentStackedBar");
        compare(item.axisMinimum, 0);
        compare(item.axisMaximum, 100);
        compare(item.normalizedValue(0, 0), 25);
        compare(item.normalizedValue(0, 1), 75);
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
