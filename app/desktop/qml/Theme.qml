pragma Singleton

import QtQuick

QtObject {
    property string themeName: "light"

    readonly property bool gruvbox: themeName === "gruvbox"
    readonly property bool dark: themeName === "dark" || gruvbox

    readonly property color window: gruvbox ? "#282828" : (dark ? "#111827" : "#f5f6f8")
    readonly property color panel: gruvbox ? "#32302f" : (dark ? "#1f2937" : "#ffffff")
    readonly property color header: gruvbox ? "#3c3836" : (dark ? "#273449" : "#e7edf4")
    readonly property color border: gruvbox ? "#665c54" : (dark ? "#39475d" : "#cfd6df")
    readonly property color text: gruvbox ? "#ebdbb2" : (dark ? "#f3f6fb" : "#1f2933")
    readonly property color mutedText: gruvbox ? "#a89984" : (dark ? "#a9b4c3" : "#5d6978")
    readonly property color accent: gruvbox ? "#d79921" : (dark ? "#3b82c4" : "#2266aa")
    readonly property color accentText: "#ffffff"
    readonly property color danger: gruvbox ? "#fb4934" : (dark ? "#ff8a80" : "#b42318")
    readonly property color rowAlt: gruvbox ? "#282828" : (dark ? "#172232" : "#eef3f8")
    readonly property string fontFamily: "Arial"
    readonly property int radius: 6
    readonly property int gap: 8
    readonly property int controlHeight: 30

    function densityValue(density, compactValue, normalValue, comfortableValue) {
        if (density === "compact") {
            return compactValue
        }
        return density === "comfortable" ? comfortableValue : normalValue
    }
}
