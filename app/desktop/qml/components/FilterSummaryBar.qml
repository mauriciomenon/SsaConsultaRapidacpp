pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SsaConsultaRapida

Rectangle {
    id: root
    required property var filterViewModel
    required property string searchText
    property bool framed: true
    signal clearSearchRequested
    signal clearAllRequested

    readonly property string trimmedSearchText: searchText.trim()
    readonly property bool hasSearch: trimmedSearchText.length > 0
    readonly property bool hasFilterEntries: filterViewModel.activeFilterEntries.length > 0
    readonly property bool hasActiveExclusion: filterViewModel.excludeScaSesSte
    // Mirrors Python filtersSummaryFrame active_state: any search, chip, or SCA/SES/STE exclusion.
    readonly property bool hasAnyActive: hasSearch || hasFilterEntries || hasActiveExclusion
    readonly property bool hasExclusionFilter: filterViewModel.hasExclusionFilter
    readonly property int tagTextSize: Theme.fontSizeBody
    readonly property color filterAccent: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)
    readonly property int frameBorderWidth: hasAnyActive && hasExclusionFilter ? Theme.summaryBorderWidthExcluded : Theme.summaryBorderWidth
    readonly property color frameBorderColor: hasAnyActive ? Theme.accent : Theme.borderSoft

    // Python _apply_frame_style: idle = input/panel border, active = accent.
    // Exclusion (!) uses double border width; drawn inside bounds (no layout shift).
    color: "transparent"
    border.width: framed ? frameBorderWidth : 0
    border.color: frameBorderColor
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
            implicitWidth: Theme.chipRemoveButtonSize
            implicitHeight: Theme.chipHeight
            font.pixelSize: Theme.fontSizeMicro
            font.bold: false
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
                    compact: false
                    tagTextSize: root.tagTextSize
                    preferredTagWidth: naturalWidth
                    tagAccent: root.filterAccent
                    onRemoveRequested: root.clearSearchRequested()
                }

                SummaryTag {
                    visible: root.hasActiveExclusion
                    text: "Exc: " + root.filterViewModel.excludedStatusCodesText
                    tooltipText: "Excluindo " + root.filterViewModel.excludedStatusCodesText
                    compact: false
                    tagTextSize: root.tagTextSize
                    preferredTagWidth: naturalWidth
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
                        compact: false
                        tagTextSize: root.tagTextSize
                        preferredTagWidth: naturalWidth
                        tagAccent: root.filterAccent
                        onRemoveRequested: root.filterViewModel.removeActiveFilter(modelData)
                    }
                }
            }
        }
    }
}
