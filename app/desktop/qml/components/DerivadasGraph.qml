pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

// Renders the derivadas relation graph (nodes + directed edges) using a Canvas.
// Mirrors the Python details_graph_renderer SVG layout.
Flickable {
    id: root
    required property var graphModel

    contentWidth: Math.max(width, graphModel ? graphModel.graphWidth : 0)
    contentHeight: Math.max(height, graphModel ? graphModel.graphHeight : 0)
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.horizontal: ScrollBar {
        policy: ScrollBar.AsNeeded
    }
    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AsNeeded
    }

    Canvas {
        id: canvas
        x: 0
        y: 0
        width: root.contentWidth
        height: root.contentHeight

        readonly property real nodeWidth: 100
        readonly property real nodeHeight: 30

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();

            const model = root.graphModel;
            if (!model) {
                return;
            }

            // Edges first (so nodes draw on top).
            ctx.lineWidth = 0.9;
            ctx.strokeStyle = Theme.border;
            const edgeList = model.edges();
            for (const edge of edgeList) {
                ctx.beginPath();
                ctx.moveTo(edge.fromX, edge.fromY);
                ctx.lineTo(edge.toX, edge.toY);
                if (edge.dashed) {
                    ctx.setLineDash([7, 6]);
                } else {
                    ctx.setLineDash([]);
                }
                ctx.stroke();
                ctx.setLineDash([]);
            }

            // Nodes.
            const count = model.rowCount();
            for (let i = 0; i < count; ++i) {
                const center = model.nodeCenter(i);
                const ssa = model.nodeSsa(i);
                const isTarget = model.nodeIsTarget(i);
                const cx = center.x;
                const cy = center.y;
                const x0 = cx - canvas.nodeWidth / 2;
                const y0 = cy - canvas.nodeHeight / 2;

                ctx.beginPath();
                ctx.roundedRect(x0, y0, canvas.nodeWidth, canvas.nodeHeight, 5, 5);
                ctx.fillStyle = isTarget ? Theme.accent : Theme.surface;
                ctx.fill();
                ctx.lineWidth = 0.8;
                ctx.strokeStyle = isTarget ? Theme.accentStrong : Theme.border;
                ctx.stroke();

                ctx.fillStyle = isTarget ? Theme.accentText : Theme.text;
                ctx.font = "bold 12px " + Theme.fontFamily;
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";
                ctx.fillText(ssa, cx, cy);
            }
        }
    }

    Connections {
        target: root.graphModel
        function onGraphChanged() {
            canvas.requestPaint();
        }
    }
}
