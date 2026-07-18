import QtQuick
import SsaConsultaRapida

AnalyticsChart {
    id: root

    property var chartModel: ({})

    readonly property var presentedSeries: presentSeries()

    function seriesLabel(name) {
        const labels = {
            "total": qsTr("Total"),
            "registered_in_period": qsTr("Cadastradas no periodo"),
            "registered_before_period": qsTr("Cadastradas anteriormente"),
            "registration_unknown": qsTr("Cadastro sem data conhecida"),
            "on_time": qsTr("No prazo"),
            "warning": qsTr("Alerta"),
            "overdue": qsTr("Fora do prazo")
        };
        return labels[name] === undefined ? name : labels[name];
    }

    function seriesColor(name) {
        const colors = {
            "registered_in_period": "#4c78a8",
            "registered_before_period": "#d95f5f",
            "registration_unknown": "#9aa0a6",
            "on_time": "#2e7d32",
            "warning": "#f9a825",
            "overdue": "#c62828"
        };
        return colors[name];
    }

    function presentSeries() {
        const input = root.chartModel && root.chartModel.series ? root.chartModel.series : [];
        const output = [];
        for (let index = 0; index < input.length; ++index) {
            const item = input[index];
            const name = item && item.name !== undefined ? String(item.name) : "";
            const presented = {
                "name": root.seriesLabel(name),
                "values": item && item.values ? item.values : [],
                "trendValues": item && item.trendValues ? item.trendValues : []
            };
            const color = root.seriesColor(name);
            if (color !== undefined)
                presented.color = color;
            output.push(presented);
        }
        return output;
    }

    function presentedQualityPart(part) {
        if (part.indexOf("excluded_for_data_quality=") === 0)
            return qsTr("Itens fora do denominador: %1").arg(part.split("=")[1]);
        if (part.indexOf("snapshot_stale_by_weeks=") === 0)
            return qsTr("Captura atrasada em %1 semana(s)").arg(part.split("=")[1]);
        return root.presentedUnavailableReason(part);
    }

    function presentedQualityText() {
        const text = root.chartModel && root.chartModel.qualityText ? String(root.chartModel.qualityText) : "";
        const parts = text.split(" | ");
        const presented = [];
        for (let index = 0; index < parts.length; ++index)
            presented.push(root.presentedQualityPart(parts[index]));
        return presented.join(" | ");
    }

    function presentedUnavailableReason(reason) {
        const labels = {
            "complete partial-attention source is unavailable": qsTr("fonte completa de atencao parcial nao disponivel"),
            "warning window is required for deadline analytics": qsTr("informe a janela de alerta para analisar prazos"),
            "analytics snapshot is incomplete": qsTr("captura analitica incompleta"),
            "snapshot history is unavailable": qsTr("historico de capturas indisponivel"),
            "analytics_result_incomplete": qsTr("resultado analitico incompleto"),
            "projection_unavailable": qsTr("projecao analitica indisponivel")
        };
        return labels[reason] === undefined ? reason : labels[reason];
    }

    function unavailableMessage() {
        if (!root.chartModel || root.chartModel.available !== false)
            return qsTr("Sem dados disponiveis para o periodo selecionado");
        const reason = root.chartModel.unavailableReason ? root.presentedUnavailableReason(String(root.chartModel.unavailableReason)) : qsTr("Fonte indisponivel");
        return qsTr("Indisponivel: %1").arg(reason);
    }

    categories: chartModel && chartModel.categories ? chartModel.categories : []
    series: presentedSeries
    trendValues: chartModel && chartModel.trendValues ? chartModel.trendValues : []
    subtitle: chartModel && chartModel.subtitle ? String(chartModel.subtitle) : ""
    qualityText: presentedQualityText()
    emptyMessage: unavailableMessage()
    valueSuffix: chartType === "percentStackedBar" ? "%" : ""
    xAxisTitle: chartType === "trendLine" ? qsTr("Tempo") : qsTr("Categoria")
    yAxisTitle: chartType === "percentStackedBar" ? qsTr("Percentual") : qsTr("SSAs")
}
