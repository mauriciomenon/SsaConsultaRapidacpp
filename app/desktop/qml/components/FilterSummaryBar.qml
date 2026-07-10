pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property string searchText
    signal clearSearchRequested
    signal clearAllRequested

    readonly property string trimmedSearchText: searchText.trim()
    readonly property bool hasSearch: trimmedSearchText.length > 0
    readonly property bool hasFilterEntries: filterViewModel.activeFilterEntries.length > 0
    readonly property bool hasActiveExclusion: filterViewModel.excludeScaSesSte
    // Mirrors Python filtersSummaryFrame active_state: any search, chip, or SCA/SES/STE exclusion.
    readonly property bool hasAnyActive: hasSearch || hasFilterEntries || hasActiveExclusion
    readonly property int activeTagCount: filterViewModel.activeFilterEntries.length + (hasSearch ? 1 : 0) + (hasActiveExclusion ? 1 : 0)
    readonly property bool compact: activeTagCount >= 2
    readonly property int tagTextSize: compact ? 11 : 12
    readonly property color filterAccent: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)

    // Python _apply_frame_style: idle = input/panel border, active = accent.
    color: "transparent"
    border.width: 1
    border.color: hasAnyActive ? Theme.accent : Theme.borderSoft
    radius: Theme.radius
    clip: false

    Row {
        id: tagGroup
        anchors.left: parent.left
        anchors.leftMargin: Theme.summaryLeftMargin
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height - Theme.spacingMd
        spacing: Theme.summaryTagSpacing

        ActionButton {
            id: clearSummaryButton
            text: "x"
            implicitWidth: Theme.summaryClearButtonWidth
            implicitHeight: root.compact ? Theme.chipHeightCompact : Theme.chipHeight
            enabled: root.hasAnyActive
            ToolTip.visible: hovered
            ToolTip.text: "Limpar filtros"
            ToolTip.delay: 0
            onClicked: root.clearAllRequested()
        }

        ScrollView {
            id: summaryScroller
            readonly property int maxAvailableWidth: Math.max(Theme.summaryMinWidth, root.width - clearSummaryButton.width - tagGroup.spacing - Theme.summaryClearButtonOffset)
            width: maxAvailableWidth
            height: parent.height
            clip: true
            contentHeight: availableHeight
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: summaryTags.width > width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

            Item {
                id: summaryContent
                height: parent.availableHeight
                width: Math.max(summaryScroller.width, summaryTags.implicitWidth)

                Label {
                    visible: !root.hasAnyActive
                    anchors.fill: parent
                    text: "Sem filtros manuais"
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontSizeLabel
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Row {
                    id: summaryTags
                    height: parent.height
                    spacing: Theme.summaryTagSpacing

                    SummaryTag {
                        visible: root.hasSearch
                        text: "Busca: '" + root.trimmedSearchText + "'"
                        tooltipText: root.trimmedSearchText
                        compact: root.compact
                        tagTextSize: root.tagTextSize
                        tagAccent: root.filterAccent
                        onRemoveRequested: root.clearSearchRequested()
                    }

                    SummaryTag {
                        visible: root.hasActiveExclusion
                        text: "Exc: SCA/SES/STE"
                        tooltipText: "Excluindo SCA/SES/STE"
                        compact: root.compact
                        tagTextSize: root.tagTextSize
                        tagAccent: root.filterAccent
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
                            tagAccent: root.filterAccent
                            onRemoveRequested: root.filterViewModel.removeActiveFilter(modelData)
                        }
                    }
                }
            }
        }
    }
}
