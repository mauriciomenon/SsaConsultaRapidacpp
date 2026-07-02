pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    required property var browseViewModel
    property string density: "normal"
    readonly property int titleTextSize: Theme.densityValue(root.density, 14, 16, 18)
    readonly property int labelTextSize: Theme.densityValue(root.density, 12, 13, 14)
    readonly property int valueTextSize: Theme.densityValue(root.density, 12, 13, 15)
    readonly property int detailsLabelWidth: Theme.densityValue(root.density, 104, 116, 132)
    signal openRequested
    signal graphWindowRequested
    // Emitted when the user clicks a relation node: the main view loads that
    // SSA into the details panel (not open SAM).
    signal loadRelationRequested(string ssaNumber)

    function relationBadge(role) {
        if (role === "parent")
            return "Origem";
        if (role === "current")
            return "Atual";
        if (role === "child")
            return "";
        if (role === "related")
            return "Relac.";
        return "SSA";
    }

    function relationConnector(index, role) {
        if (index <= 0)
            return "";
        const previous = root.viewModel.relations[index - 1];
        const previousRole = previous !== undefined ? previous.role : "";
        if (previousRole === "current" && role === "child")
            return "|-";
        if (previousRole === "child" && role === "child")
            return "|-";
        if (role === "related")
            return "- -";
        return "->";
    }

    function relationBorderColor(role, selected) {
        if (selected)
            return Theme.accent;
        if (role === "parent")
            return Theme.accentStrong;
        if (role === "child")
            return Theme.accent;
        if (role === "related")
            return Theme.mutedText;
        return Theme.border;
    }

    function relationAccentColor(role, selected) {
        if (selected)
            return Theme.accentStrong;
        if (role === "parent")
            return Theme.accentStrong;
        if (role === "child")
            return Theme.accent;
        if (role === "related")
            return Theme.mutedText;
        return Theme.border;
    }

    function relationFillColor(role, selected) {
        if (selected)
            return Theme.accentSoft;
        if (role === "parent")
            return Theme.surface;
        if (role === "child")
            return Theme.accentSoft;
        if (role === "related")
            return Theme.window;
        return Theme.panelRaised;
    }

    function isLongField(key) {
        return key === "descricao_ssa" || key === "descricao_execucao" || key === "justificativa" || key === "parciais";
    }

    color: Theme.panel
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: relationsLayout.implicitHeight + 6
            visible: root.viewModel.relationCount > 0
            color: Theme.surface
            border.color: Theme.border
            radius: Theme.radius
            clip: true

            ColumnLayout {
                id: relationsLayout
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.relationNodeHeight + 12
                    spacing: 6

                    Flickable {
                        id: relationsFlick
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.relationNodeHeight + 12
                        clip: true
                        contentWidth: relationsRow.width
                        contentHeight: relationsRow.height
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.HorizontalFlick

                        Row {
                            id: relationsRow
                            spacing: 5

                            Repeater {
                                model: root.viewModel.relations

                                delegate: Row {
                                    id: relationRow
                                    required property int index
                                    required property var modelData
                                    spacing: 6
                                    width: relationConnectorLabel.width + relationBox.width + spacing
                                    height: relationBox.implicitHeight

                                    Label {
                                        id: relationConnectorLabel
                                        visible: relationRow.index > 0
                                        text: root.relationConnector(relationRow.index, relationRow.modelData.role)
                                        color: relationRow.modelData.role === "related" ? Theme.mutedText : Theme.accentStrong
                                        font.bold: true
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: text.length > 0 ? implicitWidth : 2
                                    }

                                    Rectangle {
                                        id: relationBox
                                        width: Math.max(Theme.relationNodeMinWidth + 18, relationText.implicitWidth + 26)
                                        implicitHeight: Theme.relationNodeHeight + 8
                                        radius: Theme.radius
                                        color: root.relationFillColor(relationRow.modelData.role, relationRow.index === root.viewModel.currentRelationIndex)
                                        border.color: root.relationBorderColor(relationRow.modelData.role, relationRow.index === root.viewModel.currentRelationIndex)

                                        Rectangle {
                                            width: 3
                                            height: parent.height - 8
                                            anchors.left: parent.left
                                            anchors.leftMargin: 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            radius: 1
                                            color: root.relationAccentColor(relationRow.modelData.role, relationRow.index === root.viewModel.currentRelationIndex)
                                        }

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 1

                                            Text {
                                                text: root.relationBadge(relationRow.modelData.role)
                                                visible: text.length > 0
                                                color: relationRow.index === root.viewModel.currentRelationIndex ? Theme.accentStrong : root.relationBorderColor(relationRow.modelData.role, false)
                                                font.pixelSize: 9
                                                font.bold: true
                                                horizontalAlignment: Text.AlignHCenter
                                                width: relationBox.width - 12
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                id: relationText
                                                text: relationRow.modelData.ssa
                                                // The Current node sits on accentSoft. Pick the foreground that
                                                // contrasts with that specific tint across all themes.
                                                color: relationRow.index === root.viewModel.currentRelationIndex ? (Theme.isDarkTint(Theme.accentSoft) ? Theme.text : (Theme.dark ? Theme.accentText : Theme.text)) : Theme.text
                                                font.bold: true
                                                font.pixelSize: root.valueTextSize + 1
                                                horizontalAlignment: Text.AlignHCenter
                                                width: relationBox.width - 12
                                            }

                                            Text {
                                                text: {
                                                    const status = relationRow.modelData.status !== undefined ? relationRow.modelData.status : "";
                                                    if (status.length > 0)
                                                        return "<b>" + status + "</b>";
                                                    return "";
                                                }
                                                color: relationRow.index === root.viewModel.currentRelationIndex ? (Theme.isDarkTint(Theme.accentSoft) ? Theme.mutedText : (Theme.dark ? Theme.accentText : Theme.text)) : Theme.mutedText
                                                font.pixelSize: Math.max(10, root.valueTextSize - 2)
                                                textFormat: Text.RichText
                                                horizontalAlignment: Text.AlignHCenter
                                                width: relationBox.width - 12
                                                elide: Text.ElideRight
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (relationRow.index > 0) {
                                                    root.loadRelationRequested(relationRow.modelData.ssa);
                                                }
                                            }
                                            ToolTip.visible: containsMouse
                                            ToolTip.delay: 0
                                            ToolTip.text: {
                                                const s = relationRow.modelData.ssa !== undefined ? relationRow.modelData.ssa : "";
                                                const st = relationRow.modelData.status !== undefined ? relationRow.modelData.status : "";
                                                const k = relationRow.modelData.kind !== undefined ? relationRow.modelData.kind : "";
                                                return s + (st.length > 0 ? " [" + st + "]" : "") + (k.length > 0 ? " - " + k : "");
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Thin themed scrollbar only when content overflows.
                        ScrollBar.horizontal: ScrollBar {
                            policy: relationsFlick.contentWidth > relationsFlick.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                            contentItem: Rectangle {
                                implicitHeight: 2
                                color: Theme.accent
                                opacity: 0.7
                                radius: 1
                            }
                        }
                    }

                    Column {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        Layout.minimumWidth: 80
                        Layout.preferredWidth: 80
                        spacing: 1

                        ActionButton {
                            text: "Grafo"
                            anchors.horizontalCenter: parent.horizontalCenter
                            implicitWidth: 58
                            implicitHeight: 21
                            font.pixelSize: 11
                            enabled: root.viewModel.selectedSsaNumber.length > 0
                            onClicked: root.graphWindowRequested()
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 2

                            ActionButton {
                                text: "<"
                                implicitWidth: 22
                                implicitHeight: 21
                                font.pixelSize: 11
                                enabled: root.viewModel.canSelectPreviousRelation
                                onClicked: root.viewModel.selectPreviousRelation()
                            }
                            Label {
                                text: root.viewModel.relationCount > 0 ? (root.viewModel.currentRelationIndex + 1) + "/" + root.viewModel.relationCount : ""
                                color: Theme.mutedText
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                width: 30
                                height: 21
                            }
                            ActionButton {
                                text: ">"
                                implicitWidth: 22
                                implicitHeight: 21
                                font.pixelSize: 11
                                enabled: root.viewModel.canSelectNextRelation
                                onClicked: root.viewModel.selectNextRelation()
                            }
                        }
                    }
                }
            }
        }

        ListView {
            id: detailsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fieldCount > 0
            clip: true
            interactive: true
            model: root.viewModel.fields
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                id: fieldDelegate
                required property string key
                required property string label
                required property var value
                property string rowValue: value === undefined || value === null ? "" : String(value)
                property bool longField: root.isLongField(key)

                width: detailsList.width
                spacing: 0

                RowLayout {
                    width: parent.width
                    spacing: 5

                    Label {
                        Layout.preferredWidth: root.detailsLabelWidth
                        Layout.alignment: Qt.AlignTop
                        font.pixelSize: root.labelTextSize
                        font.bold: true
                        text: fieldDelegate.label + ":"
                        color: Theme.text
                        elide: Text.ElideRight
                    }

                    TextEdit {
                        Layout.fillWidth: true
                        Layout.maximumHeight: fieldDelegate.longField ? Math.max(42, root.valueTextSize * 4) : root.valueTextSize + 5
                        text: fieldDelegate.rowValue
                        color: Theme.text
                        readOnly: true
                        selectByMouse: true
                        selectedTextColor: Theme.accentText
                        selectionColor: Theme.accent
                        wrapMode: TextEdit.Wrap
                        font.pixelSize: root.valueTextSize
                        font.bold: true
                        clip: true
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                    opacity: 0.8
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.viewModel.fieldCount === 0
            text: "Selecione uma linha da tabela para ver os detalhes"
            color: Theme.mutedText
            font.pixelSize: root.valueTextSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
