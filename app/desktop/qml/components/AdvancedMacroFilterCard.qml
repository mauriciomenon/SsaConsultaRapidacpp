pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

FilterCard {
    id: root
    required property var sectorHierarchy
    required property var macro
    signal applyRequested

    Layout.fillWidth: true
    Layout.preferredHeight: root.macro.reportRows.length > 0 ? 156 : 36

    function macroIndex() {
        for (var index = 0; index < root.macro.options.length; ++index) {
            if (root.macro.options[index].value === root.macro.selectedMacro)
                return index;
        }
        return 0;
    }

    GridLayout {
        anchors.fill: parent
        columns: 3
        columnSpacing: Theme.gap
        rowSpacing: 4

        FilterFieldLabel {
            text: "Macro"
        }
        AppComboBox {
            id: macroSelector
            Layout.fillWidth: true
            textRole: "label"
            valueRole: "value"
            model: root.macro.options
            currentIndex: root.macroIndex()
            onActivated: {
                root.macro.selectedMacro = currentValue;
                root.applyRequested();
            }
        }
        Label {
            Layout.fillWidth: true
            visible: root.macro.reportTitle.length > 0
            text: root.macro.reportTitle + ": " + root.macro.reportText
            color: Theme.accentStrong
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        ColumnLayout {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2
            visible: root.macro.reportRows.length > 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                FilterFieldLabel {
                    Layout.preferredWidth: 92
                    text: "Setor/Div"
                }
                FilterFieldLabel {
                    Layout.preferredWidth: 58
                    text: "Semana"
                }
                FilterFieldLabel {
                    Layout.fillWidth: true
                    text: "Pessoa"
                }
                FilterFieldLabel {
                    Layout.preferredWidth: 42
                    text: "SSAs"
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
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.preferredWidth: 58
                                text: reportRow.modelData.week
                                color: Theme.text
                                font.pixelSize: 11
                            }
                            Label {
                                Layout.fillWidth: true
                                text: reportRow.modelData.person
                                color: Theme.text
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.preferredWidth: 42
                                horizontalAlignment: Text.AlignRight
                                text: reportRow.modelData.count
                                color: Theme.accentStrong
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
