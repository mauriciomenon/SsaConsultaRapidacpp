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
    required property var sectorHierarchy
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
                Layout.preferredWidth: Math.max(96, Math.round(root.cardWidth * 0.5))
                visible: root.macro.reportTitle.length > 0
                text: root.macro.reportText.length > 0 ? root.macro.reportText : ""
                color: Theme.accentStrong
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
                    root.macro.selectedMacro = currentValue;
                    root.applyRequested();
                }
            }

            ActionButton {
                text: "X"
                implicitWidth: 28
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.bold: true
                font.pixelSize: Theme.fontSizeBody
                ToolTip.visible: hovered
                ToolTip.text: "Limpar macro"
                ToolTip.delay: 0
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
            visible: root.macro.reportRows.length > 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.preferredWidth: 92
                    text: "Setor/Div"
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: 58
                    text: "Semana"
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                }
                Label {
                    Layout.fillWidth: true
                    text: "Pessoa"
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                    elide: Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: 42
                    text: "SSAs"
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeMicro
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: root.macro.reportRows

                        RowLayout {
                            id: reportRow
                            required property var modelData

                            width: parent.width
                            spacing: 6

                            Label {
                                Layout.preferredWidth: 92
                                text: reportRow.modelData.group
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeMicro
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.preferredWidth: 58
                                text: reportRow.modelData.week
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeMicro
                            }
                            Label {
                                Layout.fillWidth: true
                                text: reportRow.modelData.person
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeMicro
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.preferredWidth: 42
                                horizontalAlignment: Text.AlignRight
                                text: reportRow.modelData.count
                                color: Theme.accentStrong
                                font.pixelSize: Theme.fontSizeMicro
                            }
                        }
                    }
                }
            }
        }
    }
}
