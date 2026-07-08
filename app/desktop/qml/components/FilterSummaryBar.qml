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

    function estimatedTagWidth(text) {
        return Math.max(92, Math.ceil(String(text).length * root.tagTextSize * 0.64) + 52);
    }

    function tagTexts() {
        var texts = [];
        if (root.hasSearch)
            texts.push("Busca: '" + root.trimmedSearchText + "'");
        if (root.hasActiveExclusion)
            texts.push("Exc: SCA/SES/STE");
        for (var index = 0; index < root.filterViewModel.activeFilterEntries.length; ++index)
            texts.push(String(root.filterViewModel.activeFilterEntries[index].text));
        return texts;
    }

    function totalEstimatedTagWidth() {
        const texts = tagTexts();
        var total = Math.max(0, texts.length - 1) * summaryTags.spacing;
        for (var index = 0; index < texts.length; ++index)
            total += estimatedTagWidth(texts[index]);
        return total;
    }

    function preferredWidthForTag(text) {
        if (root.activeTagCount <= 0)
            return 0;
        const natural = estimatedTagWidth(text);
        const available = Math.max(0, summaryScroller.width - Math.max(0, root.activeTagCount - 1) * summaryTags.spacing);
        if (totalEstimatedTagWidth() <= summaryScroller.width)
            return natural;
        const minWidth = Math.max(92, Math.min(150, Math.floor(available / root.activeTagCount)));
        var extraNeed = 0;
        const texts = tagTexts();
        for (var index = 0; index < texts.length; ++index)
            extraNeed += Math.max(0, estimatedTagWidth(texts[index]) - minWidth);
        if (extraNeed <= 0)
            return minWidth;
        const extraAvailable = Math.max(0, available - minWidth * root.activeTagCount);
        return minWidth + Math.floor(Math.max(0, natural - minWidth) * Math.min(1, extraAvailable / extraNeed));
    }

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
                        preferredTagWidth: root.preferredWidthForTag(text)
                        tagAccent: root.filterAccent
                        onRemoveRequested: root.clearSearchRequested()
                    }

                    SummaryTag {
                        visible: root.hasActiveExclusion
                        text: "Exc: SCA/SES/STE"
                        tooltipText: "Excluindo SCA/SES/STE"
                        compact: root.compact
                        tagTextSize: root.tagTextSize
                        preferredTagWidth: root.preferredWidthForTag(text)
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
                            preferredTagWidth: root.preferredWidthForTag(text)
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
