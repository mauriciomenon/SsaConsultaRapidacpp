pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

// Execution week filter. Writes only to the dedicated executionWeekStart/End
// contract fields (NOT to the shared weekColumnKey/yearFilter/weekFilter)
// so it never collides with the Planned card. Inputs are YYYYWW (AAAAASS).
// Rule: if "De" is set and "Ate" is empty, the apply pre-fills Ate with the
// current ISO week before submitting; the visible field stays empty.
FilterCard {
    id: root
    required property var week
    required property real cardWidth
    required property real cardHeight
    signal applyRequested
    property string rangeStartDraft: root.week.executionWeekStartFilter
    property string rangeEndDraft: root.week.executionWeekEndFilter

    width: cardWidth
    height: cardHeight
    padding: 3
    color: "transparent"
    border.color: "transparent"

    function fieldBorder(valid, control) {
        if (!valid)
            return Theme.danger;
        return control.activeFocus ? Theme.accent : Theme.border;
    }

    function currentYearWeek() {
        const d = new Date();
        const t = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()));
        const day = t.getUTCDay() || 7;
        t.setUTCDate(t.getUTCDate() + 4 - day);
        const yearStart = new Date(Date.UTC(t.getUTCFullYear(), 0, 1));
        const isoWeek = 1 + Math.round(((t - yearStart) / 86400000 - 1 + (yearStart.getUTCDay() || 7)) / 7);
        const thursday = new Date(t);
        const finalYear = thursday.getUTCFullYear();
        return finalYear * 100 + Math.min(53, Math.max(1, isoWeek));
    }

    function submit() {
        if (!root.week.isYearWeekValid(root.rangeStartDraft) || !root.week.isYearWeekValid(root.rangeEndDraft))
            return;
        if (root.rangeStartDraft.length > 0 && root.rangeEndDraft.length === 0) {
            root.week.executionWeekEndFilter = String(root.currentYearWeek());
        } else {
            root.week.executionWeekEndFilter = root.rangeEndDraft;
        }
        root.week.executionWeekStartFilter = root.rangeStartDraft;
        root.applyRequested();
    }

    Connections {
        target: root.week

        function onChanged() {
            if (!rangeStartField.activeFocus)
                root.rangeStartDraft = root.week.executionWeekStartFilter;
            if (!rangeEndField.activeFocus)
                root.rangeEndDraft = root.week.executionWeekEndFilter;
        }
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
                text: "Execucao (AnoSemana)"
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(96, Math.max(60, root.cardWidth * 0.16))
                visible: root.week.executionWeekStartFilter.length > 0
                text: root.week.executionWeekStartFilter + (root.week.executionWeekEndFilter.length > 0 ? ".." + root.week.executionWeekEndFilter : "+")
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

            AppTextField {
                id: rangeStartField
                objectName: "executionWeekStartField"
                Layout.minimumWidth: 70
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.filterRowHeight
                placeholderText: "De"
                text: root.rangeStartDraft
                inputMethodHints: Qt.ImhDigitsOnly
                onTextEdited: root.rangeStartDraft = text
                onAccepted: root.submit()
                background: Rectangle {
                    color: Theme.panelRaised
                    border.color: root.fieldBorder(rangeStartField.text.length === 0 || root.week.isYearWeekValid(rangeStartField.text), rangeStartField)
                    border.width: 1
                    radius: Theme.radius
                }
            }

            AppTextField {
                id: rangeEndField
                objectName: "executionWeekEndField"
                Layout.minimumWidth: 70
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.filterRowHeight
                placeholderText: "Ate"
                text: root.rangeEndDraft
                inputMethodHints: Qt.ImhDigitsOnly
                onTextEdited: root.rangeEndDraft = text
                onAccepted: root.submit()
                background: Rectangle {
                    color: Theme.panelRaised
                    border.color: root.fieldBorder(rangeEndField.text.length === 0 || root.week.isYearWeekValid(rangeEndField.text), rangeEndField)
                    border.width: 1
                    radius: Theme.radius
                }
            }

            ActionButton {
                text: "x"
                implicitWidth: Theme.filterCommandWidth
                implicitHeight: Theme.filterRowHeight
                padding: 0
                font.bold: false
                font.pixelSize: Theme.fontSizeMicro
                Accessible.name: "Limpar filtro de semana de execucao"
                ToolTip.visible: hovered
                ToolTip.text: "Limpar Execucao"
                ToolTip.delay: 0
                onClicked: {
                    root.rangeStartDraft = "";
                    root.rangeEndDraft = "";
                    root.week.executionWeekStartFilter = "";
                    root.week.executionWeekEndFilter = "";
                    root.applyRequested();
                }
            }
        }
    }
}
