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
    signal clearAllRequested

    readonly property string trimmedSearchText: searchText.trim()
    readonly property bool hasSearch: trimmedSearchText.length > 0
    readonly property bool hasFilterEntries: filterViewModel.activeFilterEntries.length > 0
    readonly property bool hasActiveExclusion: filterViewModel.excludeScaSesSte
    // Mirrors Python filtersSummaryFrame active_state: any search, chip, or SCA/SES/STE exclusion.
    readonly property bool hasAnyActive: hasSearch || hasFilterEntries || hasActiveExclusion
    readonly property bool hasExclusionFilter: filterViewModel.hasExclusionFilter
    readonly property int activeTagCount: filterViewModel.activeFilterEntries.length + (hasSearch ? 1 : 0) + (hasActiveExclusion ? 1 : 0)
    readonly property bool compact: activeTagCount >= 2
    readonly property int tagTextSize: compact ? 11 : 12
    readonly property color filterAccent: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)
    // Mirror Python QHBoxLayout: use equal final widths when natural minima fit;
    // otherwise preserve each minimum and let ScrollView expose the overflow.
    readonly property var tagMetrics: {
        const naturalWidths = [];
        for (let index = 0; index < summaryTags.children.length; ++index) {
            const child = summaryTags.children[index];
            if (child.visible && child.naturalWidth !== undefined)
                naturalWidths.push(Number(child.naturalWidth));
        }
        const count = naturalWidths.length;
        const spacingWidth = Math.max(0, count - 1) * Theme.summaryTagSpacing;
        const availableWidth = Math.max(0, summaryScroller.availableWidth - spacingWidth);
        let totalNaturalWidth = 0;
        let maximumNaturalWidth = 0;
        for (let index = 0; index < count; ++index) {
            totalNaturalWidth += naturalWidths[index];
            maximumNaturalWidth = Math.max(maximumNaturalWidth, naturalWidths[index]);
        }
        const equalWidth = count > 0 ? availableWidth / count : 0;
        const fits = count > 0 && totalNaturalWidth <= availableWidth;
        return {
            equalWidth: equalWidth,
            fits: fits,
            useEqualWidths: fits && maximumNaturalWidth <= equalWidth,
            surplusPerTag: fits ? (availableWidth - totalNaturalWidth) / count : 0
        };
    }

    function preferredWidthForTag(naturalWidth) {
        if (!tagMetrics.fits)
            return naturalWidth;
        if (tagMetrics.useEqualWidths)
            return tagMetrics.equalWidth;
        return naturalWidth + tagMetrics.surplusPerTag;
    }

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
            Accessible.name: "Limpar todos os filtros"
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
            contentWidth: summaryTags.width
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: summaryTags.width > availableWidth ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

            RowLayout {
                id: summaryTags
                width: Math.max(summaryScroller.availableWidth, implicitWidth)
                height: summaryScroller.availableHeight
                spacing: Theme.summaryTagSpacing

                SummaryTag {
                    visible: root.hasSearch
                    text: "Busca: '" + root.trimmedSearchText + "'"
                    tooltipText: root.trimmedSearchText
                    compact: root.compact
                    tagTextSize: root.tagTextSize
                    preferredTagWidth: root.preferredWidthForTag(naturalWidth)
                    tagAccent: root.filterAccent
                    onRemoveRequested: root.clearSearchRequested()
                }

                SummaryTag {
                    visible: root.hasActiveExclusion
                    text: "Exc: SCA/SES/STE"
                    tooltipText: "Excluindo SCA/SES/STE"
                    compact: root.compact
                    tagTextSize: root.tagTextSize
                    preferredTagWidth: root.preferredWidthForTag(naturalWidth)
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
                        preferredTagWidth: root.preferredWidthForTag(naturalWidth)
                        tagAccent: root.filterAccent
                        onRemoveRequested: root.filterViewModel.removeActiveFilter(modelData)
                    }
                }
            }
        }
    }
}
