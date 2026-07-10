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
    // True when any chip carries Exc: (status !CODE, SCA/SES/STE, etc.).
    readonly property bool hasExclusionFilter: {
        if (hasActiveExclusion)
            return true;
        const entries = filterViewModel.activeFilterEntries;
        for (let i = 0; i < entries.length; ++i) {
            const text = String(entries[i].text || "");
            if (text.indexOf("Exc:") >= 0)
                return true;
        }
        return false;
    }
    readonly property int activeTagCount: filterViewModel.activeFilterEntries.length + (hasSearch ? 1 : 0) + (hasActiveExclusion ? 1 : 0)
    readonly property bool compact: activeTagCount >= 2
    readonly property int tagTextSize: compact ? 11 : 12
    readonly property color filterAccent: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)

    // Python _apply_frame_style: idle = input/panel border, active = accent.
    // Exclusion (!) uses double border width; drawn inside bounds (no layout shift).
    color: "transparent"
    border.width: hasAnyActive && hasExclusionFilter ? Theme.summaryBorderWidthExcluded : Theme.summaryBorderWidth
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

        Label {
            visible: !root.hasAnyActive
            height: parent.height
            text: qsTr("Nenhum filtro ativo")
            color: Theme.mutedText
            font.pixelSize: Theme.fontSizeLabel
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        ScrollView {
            id: summaryScroller
            visible: root.hasAnyActive
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
