pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Item {
    id: root
    required property var viewModel

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.cardGap
        spacing: Theme.gap

        AppTextField {
            id: columnFilter
            Layout.fillWidth: true
            placeholderText: "Filtrar colunas por nome ou chave"
            selectByMouse: true

            Timer {
                id: columnFilterTimer
                interval: 180
                repeat: false
                onTriggered: root.viewModel.setFilterText(columnFilter.text)
            }

            onTextChanged: columnFilterTimer.restart()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.window
            border.color: Theme.border
            radius: Theme.radius
            clip: true

            ListView {
                id: columnList
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: root.viewModel

                delegate: Rectangle {
                    id: columnDelegate
                    required property int index
                    required property string columnKey
                    required property string columnLabel
                    required property bool columnVisible
                    required property bool columnVisibilityChangeEnabled
                    required property int columnWidth

                    width: columnList.width
                    height: 38
                    color: columnDelegate.index % 2 === 0 ? Theme.panel : Theme.rowAlt
                    border.color: Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.gap
                        anchors.rightMargin: Theme.gap
                        spacing: Theme.gap

                        CheckBox {
                            checked: columnDelegate.columnVisible
                            enabled: columnDelegate.columnVisibilityChangeEnabled
                            onToggled: root.viewModel.setColumnVisibleByKey(columnDelegate.columnKey, checked)
                        }
                        Label {
                            Layout.preferredWidth: 210
                            text: columnDelegate.columnLabel
                            color: Theme.text
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: columnDelegate.columnKey
                            color: Theme.mutedText
                            elide: Text.ElideRight
                        }
                        AppSpinBox {
                            id: widthSpin
                            from: root.viewModel.minColumnWidth
                            to: root.viewModel.maxColumnWidth
                            stepSize: 5
                            value: columnDelegate.columnWidth
                            onValueModified: root.viewModel.setColumnWidth(columnDelegate.columnKey, value)
                        }
                        ActionButton {
                            text: "^"
                            implicitWidth: 34
                            enabled: columnDelegate.index > 0 && columnDelegate.columnVisible
                            Accessible.name: "Mover coluna para cima"
                            ToolTip.text: "Mover coluna para cima"
                            onClicked: root.viewModel.moveColumn(columnDelegate.index, columnDelegate.index - 1)
                        }
                        ActionButton {
                            text: "v"
                            implicitWidth: 34
                            enabled: columnDelegate.index < columnList.count - 1 && columnDelegate.columnVisible
                            Accessible.name: "Mover coluna para baixo"
                            ToolTip.text: "Mover coluna para baixo"
                            onClicked: root.viewModel.moveColumn(columnDelegate.index, columnDelegate.index + 1)
                        }
                    }
                }
            }
        }
    }
}
