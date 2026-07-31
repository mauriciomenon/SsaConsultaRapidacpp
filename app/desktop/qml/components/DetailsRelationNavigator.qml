pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var viewModel
    property string density: "normal"
    readonly property int valueTextSize: Theme.densityValue(root.density, 12, 13, 15)
    readonly property int relationNodeHeight: Theme.densityValue(root.density, 36, 40, 44)
    readonly property int relationNodeMinWidth: Theme.densityValue(root.density, 78, 86, 94)
    signal graphWindowRequested
    signal loadRelationRequested(int relationIndex)

    function relationBadge(role) {
        if (role === "parent")
            return "Origem";
        if (role === "current" || role === "child")
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
            return "";
        if (role === "related")
            return "- -";
        return "->";
    }

    function relationBorderColor(role, selected) {
        if (selected)
            return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32);
        if (role === "parent")
            return Theme.accentStrong;
        if (role === "child")
            return Theme.accent;
        if (role === "related")
            return Theme.mutedText;
        return Theme.border;
    }

    function relationAccentColor(role, selected) {
        if (selected || role === "parent")
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

    Layout.fillWidth: true
    Layout.preferredHeight: relationsLayout.implicitHeight + 6
    visible: root.viewModel.relationCount > 0 || root.viewModel.relationLoading || root.viewModel.relationError.length > 0
    color: Theme.surface
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    ColumnLayout {
        id: relationsLayout
        anchors.fill: parent
        anchors.margins: 5
        spacing: 3

        Label {
            objectName: "relationStatus"
            Layout.fillWidth: true
            visible: root.viewModel.relationLoading || root.viewModel.relationError.length > 0
            text: root.viewModel.relationError.length > 0 ? root.viewModel.relationError : "Carregando relacoes"
            textFormat: Text.PlainText
            color: root.viewModel.relationError.length > 0 ? Theme.danger : Theme.mutedText
            font.pixelSize: Theme.fontSizeMicro
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.relationNodeHeight + 12
            visible: root.viewModel.relationCount > 0
            spacing: 8

            Flickable {
                id: relationsFlick
                Layout.fillWidth: true
                Layout.preferredHeight: root.relationNodeHeight + 12
                clip: true
                contentWidth: relationsRow.width
                contentHeight: relationsRow.height
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick

                Row {
                    id: relationsRow
                    y: Math.round((relationsFlick.height - height) / 2)
                    spacing: 5

                    Repeater {
                        model: root.viewModel.relations

                        delegate: Row {
                            id: relationRow
                            required property int index
                            required property var modelData
                            spacing: 8
                            width: relationConnectorLabel.width + relationBox.width + spacing
                            height: relationBox.implicitHeight

                            Label {
                                id: relationConnectorLabel
                                visible: relationRow.index > 0
                                text: root.relationConnector(relationRow.index, relationRow.modelData.role)
                                color: relationRow.modelData.role === "related" ? Theme.mutedText : Theme.accentStrong
                                font.bold: false
                                anchors.verticalCenter: parent.verticalCenter
                                width: text.length > 0 ? implicitWidth : 2
                            }

                            Rectangle {
                                id: relationBox
                                objectName: "relationNode-" + relationRow.index
                                function activateRelation() {
                                    root.loadRelationRequested(relationRow.index);
                                }

                                width: Math.max(root.relationNodeMinWidth + 18, relationText.implicitWidth + 28)
                                implicitHeight: root.relationNodeHeight
                                radius: Theme.radius
                                color: root.relationFillColor(relationRow.modelData.role, relationRow.index === root.viewModel.currentRelationIndex)
                                border.color: root.relationBorderColor(relationRow.modelData.role, relationRow.index === root.viewModel.currentRelationIndex)
                                border.width: 1
                                activeFocusOnTab: true
                                Accessible.role: Accessible.Button
                                Accessible.name: "Abrir relacao " + relationRow.modelData.ssa
                                Accessible.onPressAction: relationBox.activateRelation()
                                Keys.onReturnPressed: relationBox.activateRelation()
                                Keys.onEnterPressed: relationBox.activateRelation()
                                Keys.onSpacePressed: relationBox.activateRelation()

                                Rectangle {
                                    width: 2
                                    height: parent.height - 12
                                    anchors.left: parent.left
                                    anchors.leftMargin: 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    radius: 1
                                    color: root.relationAccentColor(relationRow.modelData.role, relationRow.index === root.viewModel.currentRelationIndex)
                                }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 0

                                    Text {
                                        objectName: "relationNodeBadge-" + relationRow.index
                                        text: root.relationBadge(relationRow.modelData.role)
                                        visible: text.length > 0
                                        color: Theme.readableText(relationBox.color, relationRow.index === root.viewModel.currentRelationIndex ? Theme.accentStrong : root.relationBorderColor(relationRow.modelData.role, false))
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: false
                                        horizontalAlignment: Text.AlignHCenter
                                        width: relationBox.width - 12
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        id: relationText
                                        objectName: "relationNodeNumber-" + relationRow.index
                                        text: relationRow.modelData.ssa
                                        textFormat: Text.PlainText
                                        color: Theme.readableText(relationBox.color, Theme.text)
                                        font.bold: false
                                        font.pixelSize: root.valueTextSize + 1
                                        horizontalAlignment: Text.AlignHCenter
                                        width: relationBox.width - 12
                                    }

                                    Text {
                                        objectName: "relationNodeStatus-" + relationRow.index
                                        text: {
                                            const status = relationRow.modelData.status !== undefined ? relationRow.modelData.status : "";
                                            return status.length > 0 ? status : "";
                                        }
                                        visible: text.length > 0
                                        color: Theme.readableText(relationBox.color, Theme.mutedText)
                                        font.pixelSize: Math.max(9, root.valueTextSize - 3)
                                        font.bold: false
                                        textFormat: Text.PlainText
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
                                        relationBox.forceActiveFocus();
                                        relationBox.activateRelation();
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
            }

            Column {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.minimumWidth: 54
                Layout.preferredWidth: 54
                spacing: 1

                ActionButton {
                    text: "Grafo"
                    anchors.horizontalCenter: parent.horizontalCenter
                    implicitWidth: 42
                    implicitHeight: 19
                    padding: 0
                    font.pixelSize: Theme.fontSizeCaption
                    enabled: root.viewModel.selectedSsaNumber.length > 0
                    ToolTip.visible: hovered
                    ToolTip.text: "Abrir grafo"
                    ToolTip.delay: 0
                    onClicked: root.graphWindowRequested()
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 2

                    ActionButton {
                        text: "<"
                        Accessible.name: "Relacao anterior"
                        implicitWidth: 20
                        implicitHeight: 19
                        padding: 0
                        font.pixelSize: Theme.fontSizeCaption
                        enabled: root.viewModel.canSelectPreviousRelation
                        onClicked: root.viewModel.selectPreviousRelation()
                    }

                    Label {
                        text: root.viewModel.relationCount > 0 ? (root.viewModel.currentRelationIndex + 1) + "/" + root.viewModel.relationCount : ""
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontSizeMicro
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        width: 24
                        height: 19
                    }

                    ActionButton {
                        text: ">"
                        Accessible.name: "Proxima relacao"
                        implicitWidth: 20
                        implicitHeight: 19
                        padding: 0
                        font.pixelSize: Theme.fontSizeCaption
                        enabled: root.viewModel.canSelectNextRelation
                        onClicked: root.viewModel.selectNextRelation()
                    }
                }
            }
        }
    }
}
