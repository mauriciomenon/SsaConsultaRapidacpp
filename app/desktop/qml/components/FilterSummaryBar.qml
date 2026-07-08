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
    readonly property int activeTagCount: filterViewModel.activeFilterEntries.length + (hasSearch ? 1 : 0) + (hasActiveExclusion ? 1 : 0)
    readonly property bool compact: activeTagCount >= 2
    readonly property int tagTextSize: compact ? 11 : 12
    readonly property color filterAccent: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)

    color: "transparent"
    border.color: "transparent"
    radius: 0
    clip: false

    Row {
        id: tagGroup
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height - 8
        spacing: 6

        ActionButton {
            id: clearSummaryButton
            text: "x"
            implicitWidth: 46
            implicitHeight: root.compact ? 24 : 26
            enabled: root.hasSearch || root.hasFilterEntries || root.hasActiveExclusion
            ToolTip.visible: hovered
            ToolTip.text: "Limpar filtros"
            ToolTip.delay: 0
            onClicked: root.clearAllRequested()
        }

        ScrollView {
            id: summaryScroller
            readonly property int maxAvailableWidth: Math.max(220, root.width - clearSummaryButton.width - tagGroup.spacing - 12)
            width: root.activeTagCount >= 4 ? maxAvailableWidth : Math.min(maxAvailableWidth, Math.max(180, root.activeTagCount * 230))
            height: parent.height
            clip: true
            contentHeight: availableHeight
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: summaryTags.width > width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

            Item {
                id: summaryContent
                height: parent.availableHeight
                width: Math.max(180, summaryTags.implicitWidth)
                readonly property int tagSlotWidth: root.activeTagCount <= 0 ? 0 : Math.max(150, Math.min(430, Math.floor((summaryScroller.width - Math.max(0, root.activeTagCount - 1) * summaryTags.spacing) / Math.min(root.activeTagCount, 4))))

                Label {
                    visible: !root.hasSearch && !root.hasFilterEntries && !root.hasActiveExclusion
                    anchors.fill: parent
                    text: "Sem filtros manuais"
                    color: Theme.mutedText
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Row {
                    id: summaryTags
                    height: parent.height
                    spacing: 6

                    SummaryTag {
                        visible: root.hasSearch
                        text: "Busca: '" + root.trimmedSearchText + "'"
                        tooltipText: root.trimmedSearchText
                        compact: root.compact
                        tagTextSize: root.tagTextSize
                        preferredTagWidth: summaryContent.tagSlotWidth
                        tagAccent: root.filterAccent
                        onRemoveRequested: root.clearSearchRequested()
                    }

                    SummaryTag {
                        visible: root.hasActiveExclusion
                        text: "Exc: SCA/SES/STE"
                        tooltipText: "Excluindo SCA/SES/STE"
                        compact: root.compact
                        tagTextSize: root.tagTextSize
                        preferredTagWidth: summaryContent.tagSlotWidth
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
                            preferredTagWidth: summaryContent.tagSlotWidth
                            tagAccent: root.filterAccent
                            onRemoveRequested: root.filterViewModel.removeActiveFilter(modelData)
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
    }
}
