pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

// Emission week filter. Writes only to the dedicated issueWeekStart/End
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
    property string rangeStartDraft: root.week.issueWeekStartFilter
    property string rangeEndDraft: root.week.issueWeekEndFilter

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

    function submit() {
        // Capture both drafts first: each setter emits changed() synchronously and
        // the Connections handler below rewrites the unfocused drafts.
        const start = root.rangeStartDraft;
        const end = root.rangeEndDraft;
        if (!root.week.isYearWeekValid(start) || !root.week.isYearWeekValid(end))
            return;
        root.week.issueWeekEndFilter = start.length > 0 && end.length === 0 ? String(root.week.currentYearWeek()) : end;
        root.week.issueWeekStartFilter = start;
        root.applyRequested();
    }

    Connections {
        target: root.week

        function onChanged() {
            if (!rangeStartField.activeFocus)
                root.rangeStartDraft = root.week.issueWeekStartFilter;
            if (!rangeEndField.activeFocus)
                root.rangeEndDraft = root.week.issueWeekEndFilter;
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
                text: "Emissao (AnoSemana)"
                color: Theme.text
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
            }

            Label {
                Layout.preferredWidth: Math.min(96, Math.max(60, root.cardWidth * 0.16))
                visible: root.week.issueWeekStartFilter.length > 0
                text: root.week.issueWeekStartFilter + (root.week.issueWeekEndFilter.length > 0 ? ".." + root.week.issueWeekEndFilter : "+")
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
                objectName: "issueWeekStartField"
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
                objectName: "issueWeekEndField"
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
                Accessible.name: "Limpar filtro de semana de emissao"
                ToolTip.visible: hovered
                ToolTip.text: "Limpar Emissao"
                ToolTip.delay: 0
                onClicked: {
                    root.rangeStartDraft = "";
                    root.rangeEndDraft = "";
                    root.week.issueWeekStartFilter = "";
                    root.week.issueWeekEndFilter = "";
                    root.applyRequested();
                }
            }
        }
    }
}
