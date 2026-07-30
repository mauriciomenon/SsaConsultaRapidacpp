pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

// Same skeleton as AdvancedTextFilterCard (ColumnLayout with two RowLayouts)
// so the "Macro" title lands on the same baseline as the other filter cards.
// The report table, when visible, is an extra child of the ColumnLayout and
// does not affect the title position.
FilterCard {
    id: root
    required property var macro
    required property real cardWidth
    required property real cardHeight
    signal applyRequested

    width: cardWidth
    height: cardHeight
    padding: 3
    color: "transparent"
    border.color: "transparent"

    function macroIndex() {
        for (var index = 0; index < root.macro.options.length; ++index) {
            if (root.macro.options[index].value === root.macro.selectedMacro)
                return index;
        }
        return 0;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 3

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: "Macro"
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(96, Math.max(60, root.cardWidth * 0.16))
                visible: root.macro.reportTitle.length > 0 || root.macro.reportLoading || root.macro.reportError.length > 0
                text: root.macro.reportLoading ? "Carregando" : root.macro.reportText
                textFormat: Text.PlainText
                color: root.macro.reportError.length > 0 ? Theme.danger : Theme.accentStrong
                font.pixelSize: Theme.fontSizeMicro
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.filterRowHeight
            spacing: 4

            AppComboBox {
                id: macroSelector
                Layout.minimumWidth: Theme.valueMinWidth
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.filterRowHeight
                leftPadding: 7
                rightPadding: 16
                textRole: "label"
                valueRole: "value"
                model: root.macro.options
                currentIndex: root.macroIndex()
                displayText: root.macro.selectedMacro.length > 0 ? root.macro.selectedMacro : "Macro"
                onActivated: {
                    const selectedValue = currentValue;
                    root.macro.selectedMacro = selectedValue;
                    if (selectedValue === "ssas_para_baixar")
                        root.applyRequested();
                }
            }

            ActionButton {
                text: "x"
                implicitWidth: Theme.filterCommandWidth
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.bold: false
                font.pixelSize: Theme.fontSizeMicro
                ToolTip.visible: hovered
                ToolTip.text: "Limpar macro"
                ToolTip.delay: 0
                Accessible.name: "Limpar macro"
                onClicked: {
                    root.macro.selectedMacro = "";
                    root.applyRequested();
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2
            visible: root.macro.reportRows.length > 0 || root.macro.reportLoading || root.macro.reportError.length > 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.preferredWidth: 92
                    text: "Setor/Div"
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: 58
                    text: "Semana"
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                }
                Label {
                    Layout.fillWidth: true
                    text: "Pessoa"
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: 42
                    text: "SSAs"
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.macro.reportLoading || root.macro.reportError.length > 0
                text: root.macro.reportLoading ? "Carregando relatorio" : root.macro.reportError
                textFormat: Text.PlainText
                color: root.macro.reportError.length > 0 ? Theme.danger : Theme.mutedText
                font.pixelSize: Theme.fontSizeMicro
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            ListView {
                id: macroReportList
                objectName: "macroReportList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                reuseItems: true
                model: root.macro.reportRows
                ScrollBar.vertical: ScrollBar {}

                delegate: RowLayout {
                    id: reportRow
                    objectName: "macroReportRow"
                    required property var modelData
                    width: ListView.view.width
                    height: 18
                    spacing: 6

                    Label {
                        Layout.preferredWidth: 92
                        text: reportRow.modelData.group
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeMicro
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.preferredWidth: 58
                        text: reportRow.modelData.week
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeMicro
                    }
                    Label {
                        Layout.fillWidth: true
                        text: reportRow.modelData.person
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeMicro
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.preferredWidth: 42
                        horizontalAlignment: Text.AlignRight
                        text: reportRow.modelData.count
                        textFormat: Text.PlainText
                        color: Theme.accentStrong
                        font.pixelSize: Theme.fontSizeMicro
                    }
                }
            }
        }
    }
}
