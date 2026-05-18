pragma Singleton

import QtQuick

QtObject {
    property string themeName: "gruvbox"

    readonly property bool gruvbox: themeName === "gruvbox"

    readonly property var gruvboxPalette: ({
        isDark: true,
        window: "#232323",
        surface: "#2a2928",
        panel: "#302b29",
        panelRaised: "#3a3330",
        header: "#2b2b2b",
        tableHeader: "#343230",
        border: "#66584f",
        borderSoft: "#403a36",
        text: "#ebdbb2",
        mutedText: "#b8a98d",
        accent: "#ffb000",
        accentText: "#241f1d",
        accentSoft: "#4a3c25",
        accentStrong: "#ffd37a",
        link: "#42a5ff",
        danger: "#fb4934",
        dangerSoft: "#4a2d29",
        dangerStrong: "#ff7d6d",
        rowAlt: "#292929",
        rowSelected: "#3f382f"
    })
    readonly property var darkPalette: ({
        isDark: true,
        window: "#0e1420",
        surface: "#111827",
        panel: "#1f2937",
        panelRaised: "#253247",
        header: "#273449",
        tableHeader: "#273449",
        border: "#374151",
        borderSoft: "#2b3748",
        text: "#f3f6fb",
        mutedText: "#a0aec0",
        accent: "#5fa6f5",
        accentText: "#08111f",
        accentSoft: "#1d426c",
        accentStrong: "#76b2ff",
        link: "#70b7ff",
        danger: "#ff8a80",
        dangerSoft: "#552b31",
        dangerStrong: "#ff8181",
        rowAlt: "#172232",
        rowSelected: "#243b5b"
    })
    readonly property var lightPalette: ({
        isDark: false,
        window: "#f1f4f9",
        surface: "#edf1f6",
        panel: "#ffffff",
        panelRaised: "#ffffff",
        header: "#e9eff8",
        tableHeader: "#e3ebf5",
        border: "#c8d4e2",
        borderSoft: "#dbe3ed",
        text: "#101826",
        mutedText: "#4e5a66",
        accent: "#1f4e99",
        accentText: "#ffffff",
        accentSoft: "#d6e7ff",
        accentStrong: "#183f83",
        link: "#0b63ce",
        danger: "#b42318",
        dangerSoft: "#f9d8d3",
        dangerStrong: "#9a1818",
        rowAlt: "#f6f8fc",
        rowSelected: "#dceaff"
    })
    readonly property var palette: gruvbox ? gruvboxPalette : (themeName === "dark" ? darkPalette : lightPalette)
    readonly property bool dark: palette.isDark

    readonly property color window: palette.window
    readonly property color surface: palette.surface
    readonly property color panel: palette.panel
    readonly property color panelRaised: palette.panelRaised
    readonly property color header: palette.header
    readonly property color tableHeader: palette.tableHeader
    readonly property color border: palette.border
    readonly property color borderSoft: palette.borderSoft
    readonly property color text: palette.text
    readonly property color mutedText: palette.mutedText
    readonly property color accent: palette.accent
    readonly property color accentText: palette.accentText
    readonly property color accentSoft: palette.accentSoft
    readonly property color accentStrong: palette.accentStrong
    readonly property color link: palette.link
    readonly property color danger: palette.danger
    readonly property color dangerSoft: palette.dangerSoft
    readonly property color dangerStrong: palette.dangerStrong
    readonly property color rowAlt: palette.rowAlt
    readonly property color rowSelected: palette.rowSelected
    readonly property string fontFamily: "Arial"
    readonly property int radius: 4
    readonly property int radiusSoft: 6
    readonly property int gap: 8
    readonly property int cardGap: 10
    readonly property int controlHeight: 30
    readonly property int panelPadding: 12
    readonly property int detailsLabelWidth: 190
    readonly property int relationNodeMinWidth: 92
    readonly property int relationNodeHeight: 40
    readonly property int bottomPaneMinHeight: 300
    readonly property int bottomPaneMaxHeight: 390
    readonly property real bottomPaneHeightRatio: 0.37

    function densityValue(density, compactValue, normalValue, comfortableValue) {
        if (density === "normal") {
            return normalValue
        }
        return density === "comfortable" ? comfortableValue : compactValue
    }

    function bottomPaneHeight(windowHeight) {
        return Math.max(bottomPaneMinHeight,
                        Math.min(bottomPaneMaxHeight,
                                 Math.round(windowHeight * bottomPaneHeightRatio)))
    }
}
