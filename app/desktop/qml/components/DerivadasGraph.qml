pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

// Renders the derivadas relation graph (nodes + directed edges) using a Canvas.
// Mirrors the Python details_graph_renderer SVG layout.
Flickable {
    id: root
    required property var graphModel
    signal nodeClicked(string ssaNumber)
    signal exportFinished(bool succeeded)

    function localPathFromUrl(fileUrl) {
        let path = decodeURIComponent(String(fileUrl));
        if (path.startsWith("file://")) {
            path = path.substring(7);
        }
        if (Qt.platform.os === "windows" && path.length > 2 && path[0] === "/" && path[2] === ":") {
            path = path.substring(1);
        }
        return path;
    }

    function savePng(fileUrl) {
        if (!graphModel || graphModel.nodeCount === 0) {
            root.exportFinished(false);
            return;
        }
        canvas.grabToImage(result => {
            root.exportFinished(result.saveToFile(root.localPathFromUrl(fileUrl)));
        });
    }

    function roleLabel(role) {
        if (role === "parent")
            return "Origem";
        if (role === "current")
            return "";
        if (role === "child")
            return "";
        if (role === "related")
            return "Relac.";
        return "SSA";
    }

    function nodeFillColor(role, isTarget) {
        if (isTarget)
            return Theme.accent;
        if (role === "parent")
            return Theme.surface;
        if (role === "child")
            return Theme.panelRaised;
        if (role === "related")
            return Theme.window;
        return Theme.surface;
    }

    function nodeStrokeColor(role, isTarget) {
        if (isTarget)
            return Theme.accentStrong;
        if (role === "parent")
            return Theme.accentStrong;
        if (role === "child")
            return Theme.accentStrong;
        if (role === "related")
            return Theme.mutedText;
        return Theme.border;
    }

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

        readonly property real nodeWidth: 118
        readonly property real nodeHeight: 52

        function nodeAt(pointX, pointY) {
            const model = root.graphModel;
            if (!model) {
                return "";
            }
            const count = model.rowCount();
            for (let i = 0; i < count; ++i) {
                const center = model.nodeCenter(i);
                const left = center.x - canvas.nodeWidth / 2;
                const right = center.x + canvas.nodeWidth / 2;
                const top = center.y - canvas.nodeHeight / 2;
                const bottom = center.y + canvas.nodeHeight / 2;
                if (pointX >= left && pointX <= right && pointY >= top && pointY <= bottom) {
                    return model.nodeSsa(i);
                }
            }
            return "";
        }

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();

            const model = root.graphModel;
            if (!model) {
                return;
            }

            // Edges first (so nodes draw on top). Right-angle (L-shaped)
            // connections: horizontal then vertical. Dashed = related,
            // solid = derived.
            ctx.lineWidth = 0.9;
            ctx.strokeStyle = Theme.border;
            const edgeList = model.edges();
            for (const edge of edgeList) {
                ctx.beginPath();
                ctx.moveTo(edge.fromX, edge.fromY);
                if (edge.routeY !== undefined) {
                    ctx.lineTo(edge.fromX, edge.routeY);
                    ctx.lineTo(edge.toX, edge.routeY);
                    ctx.lineTo(edge.toX, edge.toY);
                } else {
                    const midX = (edge.fromX + edge.toX) / 2;
                    ctx.lineTo(midX, edge.fromY);
                    ctx.lineTo(midX, edge.toY);
                    ctx.lineTo(edge.toX, edge.toY);
                }
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
                const status = model.nodeStatus(i);
                const role = model.nodeRole(i);
                const isTarget = model.nodeIsTarget(i);
                const cx = center.x;
                const cy = center.y;
                const x0 = cx - canvas.nodeWidth / 2;
                const y0 = cy - canvas.nodeHeight / 2;

                ctx.beginPath();
                ctx.roundedRect(x0, y0, canvas.nodeWidth, canvas.nodeHeight, 5, 5);
                ctx.fillStyle = root.nodeFillColor(role, isTarget);
                ctx.fill();
                ctx.lineWidth = isTarget ? 1.2 : 0.9;
                ctx.strokeStyle = root.nodeStrokeColor(role, isTarget);
                ctx.stroke();

                ctx.fillStyle = isTarget ? Theme.accentText : Theme.text;
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";
                const visibleRole = root.roleLabel(role);
                if (visibleRole.length > 0) {
                    ctx.font = "bold 10px " + Theme.fontFamily;
                    ctx.fillStyle = isTarget ? Theme.accentText : root.nodeStrokeColor(role, false);
                    ctx.fillText(visibleRole, cx, cy - 16);
                }
                ctx.font = "bold 14px " + Theme.fontFamily;
                ctx.fillStyle = isTarget ? Theme.accentText : Theme.text;
                ctx.fillText(ssa, cx, status.length > 0 ? cy - 1 : cy + 3);
                if (status.length > 0) {
                    ctx.font = "bold 12px " + Theme.fontFamily;
                    ctx.fillText(status, cx, cy + 16);
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: mouse => {
                const ssaNumber = canvas.nodeAt(mouse.x, mouse.y);
                if (ssaNumber.length > 0) {
                    root.nodeClicked(ssaNumber);
                }
            }
        }
    }

    Connections {
        target: root.graphModel
        function onGraphChanged() {
            canvas.requestPaint();
        }
    }

    Connections {
        target: Theme
        function onThemeNameChanged() {
            canvas.requestPaint();
        }
    }
}
