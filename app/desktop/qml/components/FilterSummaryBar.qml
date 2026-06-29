pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property string searchText
    signal clearSearchRequested

    readonly property string trimmedSearchText: searchText.trim()
    readonly property bool hasSearch: trimmedSearchText.length > 0
    readonly property bool hasFilterEntries: filterViewModel.activeFilterEntries.length > 0
    readonly property bool hasActiveExclusion: filterViewModel.excludeScaSesSte
    readonly property bool compact: filterViewModel.activeFilterEntries.length + (hasSearch ? 1 : 0) + (hasActiveExclusion ? 1 : 0) >= 2
    readonly property int tagTextSize: compact ? 11 : 12

    color: Theme.panelRaised
    border.color: Theme.border
    radius: Theme.radius
    clip: true

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: Theme.gap

        Label {
            visible: !root.hasSearch && !root.hasFilterEntries && !root.hasActiveExclusion
            Layout.fillWidth: true
            text: "Sem filtros manuais"
            color: Theme.mutedText
            font.pixelSize: 13
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        ScrollView {
            visible: root.hasSearch || root.hasFilterEntries || root.hasActiveExclusion
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: availableHeight
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: summaryTags.width > width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

            Row {
                id: summaryTags
                height: parent.availableHeight
                spacing: 6

                SummaryTag {
                    visible: root.hasSearch
                    text: root.trimmedSearchText
                    tooltipText: root.trimmedSearchText
                    compact: root.compact
                    tagTextSize: root.tagTextSize
                    onRemoveRequested: root.clearSearchRequested()
                }

                SummaryTag {
                    visible: root.hasActiveExclusion
                    text: "Excluindo SCA/SES/STE"
                    tooltipText: text
                    compact: root.compact
                    tagTextSize: root.tagTextSize
                    onRemoveRequested: {
                        root.filterViewModel.excludeScaSesSte = false;
                    }
                }

                Repeater {
                    model: root.filterViewModel.activeFilterEntries

                    delegate: SummaryTag {
                        required property var modelData
                        text: modelData.text
                        tooltipText: modelData.text
                        compact: root.compact
                        tagTextSize: root.tagTextSize
                        onRemoveRequested: root.filterViewModel.removeActiveFilter(modelData)
                    }
                }
            }
        }
    }
}
