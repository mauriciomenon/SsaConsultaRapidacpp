pragma Singleton

import QtQuick

QtObject {
    property bool dark: false

    readonly property color window: dark ? "#111827" : "#f5f6f8"
    readonly property color panel: dark ? "#1f2937" : "#ffffff"
    readonly property color header: dark ? "#273449" : "#e7edf4"
    readonly property color border: dark ? "#39475d" : "#cfd6df"
    readonly property color text: dark ? "#f3f6fb" : "#1f2933"
    readonly property color mutedText: dark ? "#a9b4c3" : "#5d6978"
    readonly property color accent: dark ? "#3b82c4" : "#2266aa"
    readonly property color accentText: "#ffffff"
    readonly property color danger: dark ? "#ff8a80" : "#b42318"
    readonly property color rowAlt: dark ? "#172232" : "#eef3f8"
    readonly property string fontFamily: "Arial"
    readonly property int radius: 6
    readonly property int gap: 8
    readonly property int controlHeight: 30
}
