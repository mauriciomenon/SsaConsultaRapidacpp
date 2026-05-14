pragma Singleton

import QtQuick

QtObject {
    property string themeName: "light"

    readonly property bool gruvbox: themeName === "gruvbox"
    readonly property bool dark: themeName === "dark" || gruvbox

    readonly property color window: gruvbox ? "#262423" : (dark ? "#0e1420" : "#f1f4f9")
    readonly property color surface: gruvbox ? "#2d2a28" : (dark ? "#111827" : "#edf1f6")
    readonly property color panel: gruvbox ? "#32302f" : (dark ? "#1f2937" : "#ffffff")
    readonly property color header: gruvbox ? "#3c3836" : (dark ? "#273449" : "#e9eff8")
    readonly property color border: gruvbox ? "#665c54" : (dark ? "#374151" : "#c8d4e2")
    readonly property color text: gruvbox ? "#ebdbb2" : (dark ? "#f3f6fb" : "#101826")
    readonly property color mutedText: gruvbox ? "#a89984" : (dark ? "#a0aec0" : "#4e5a66")
    readonly property color accent: gruvbox ? "#d79921" : (dark ? "#5fa6f5" : "#1f4e99")
    readonly property color accentText: gruvbox || dark ? "#fbf1c7" : "#ffffff"
    readonly property color accentSoft: gruvbox ? "#fabd2f" : (dark ? "#8cc0fa" : "#d6e7ff")
    readonly property color accentStrong: gruvbox ? "#fbf1c7" : (dark ? "#76b2ff" : "#183f83")
    readonly property color danger: gruvbox ? "#fb4934" : (dark ? "#ff8a80" : "#b42318")
    readonly property color dangerSoft: gruvbox ? "#fb4934" : (dark ? "#ffb4b4" : "#f9d8d3")
    readonly property color dangerStrong: gruvbox ? "#cc241d" : (dark ? "#ff8181" : "#9a1818")
    readonly property color rowAlt: gruvbox ? "#2b2a26" : (dark ? "#172232" : "#f6f8fc")
    readonly property string fontFamily: "Arial"
    readonly property int radius: 10
    readonly property int radiusSoft: 16
    readonly property int gap: 10
    readonly property int cardGap: 12
    readonly property int controlHeight: 32
    readonly property int panelPadding: 14

    function densityValue(density, compactValue, normalValue, comfortableValue) {
        if (density === "compact") {
            return compactValue
        }
        return density === "comfortable" ? comfortableValue : normalValue
    }
}
