pragma Singleton

import QtQuick

QtObject {
    property string themeName: "gruvbox"

    readonly property bool gruvbox: themeName === "gruvbox"

    readonly property var palettes: ({
            "grayscale": {
                isDark: true,
                window: "#2b2e33",
                surface: "#2f3238",
                panel: "#2b2e33",
                panelRaised: "#3a3d44",
                header: "#31343a",
                tableHeader: "#31343a",
                border: "#4a4d52",
                borderSoft: "#3a3d44",
                text: "#f0f0f0",
                mutedText: "#cdd0d4",
                accent: "#e6e6e6",
                accentText: "#1c1c1c",
                accentSoft: "#cfd3d6",
                accentStrong: "#ffffff",
                link: "#cfd3d6",
                danger: "#ff7d7d",
                dangerSoft: "#4a2d2d",
                dangerStrong: "#ff9a9a",
                rowAlt: "#31343a",
                rowSelected: "#3a3d44"
            },
            "windows7": {
                isDark: false,
                window: "#eef4ff",
                surface: "#ffffff",
                panel: "#f8fbff",
                panelRaised: "#ffffff",
                header: "#e3ecff",
                tableHeader: "#d8e5fb",
                border: "#d8e5fb",
                borderSoft: "#e8f0ff",
                text: "#1a1a1a",
                mutedText: "#3c4a6f",
                accent: "#2b6cc0",
                accentText: "#ffffff",
                accentSoft: "#d9ecff",
                accentStrong: "#1f56a0",
                link: "#2b6cc0",
                danger: "#b42318",
                dangerSoft: "#f8d7d2",
                dangerStrong: "#8f1b12",
                rowAlt: "#f8fbff",
                rowSelected: "#d9ecff"
            },
            "classico": {
                isDark: false,
                window: "#f7f6f5",
                surface: "#ffffff",
                panel: "#f6f5f4",
                panelRaised: "#fdfdfd",
                header: "#dfe2e4",
                tableHeader: "#f0efed",
                border: "#d7d4d1",
                borderSoft: "#e6e3df",
                text: "#2e3436",
                mutedText: "#4c5153",
                accent: "#3584e4",
                accentText: "#ffffff",
                accentSoft: "#d9ecff",
                accentStrong: "#1f66c2",
                link: "#3584e4",
                danger: "#b42318",
                dangerSoft: "#f8d7d2",
                dangerStrong: "#8f1b12",
                rowAlt: "#f0efed",
                rowSelected: "#d9ecff"
            },
            "gruvbox": {
                isDark: true,
                window: "#282828",
                surface: "#1d2021",
                panel: "#3c3836",
                panelRaised: "#504945",
                header: "#32302f",
                tableHeader: "#3c3836",
                border: "#665c54",
                borderSoft: "#504945",
                text: "#ebdbb2",
                mutedText: "#d5c4a1",
                accent: "#fabd2f",
                accentText: "#282828",
                accentSoft: "#504945",
                accentStrong: "#fecc55",
                link: "#83a598",
                danger: "#fb4934",
                dangerSoft: "#4a2d29",
                dangerStrong: "#ff7d6d",
                rowAlt: "#32302f",
                rowSelected: "#504945"
            },
            "dark": {
                isDark: true,
                window: "#121212",
                surface: "#1e1e1e",
                panel: "#2a2a2a",
                panelRaised: "#333333",
                header: "#252525",
                tableHeader: "#2a2a2a",
                border: "#4d4d4d",
                borderSoft: "#3f3f3f",
                text: "#f0f0f0",
                mutedText: "#c2c2c2",
                accent: "#7a7a7a",
                accentText: "#ffffff",
                accentSoft: "#9c9c9c",
                accentStrong: "#b5b5b5",
                link: "#7a7a7a",
                danger: "#ff8a80",
                dangerSoft: "#552b31",
                dangerStrong: "#ff8181",
                rowAlt: "#252525",
                rowSelected: "#3f3f3f"
            },
            "dracula": {
                isDark: true,
                window: "#282a36",
                surface: "#1e1f29",
                panel: "#303446",
                panelRaised: "#44475a",
                header: "#303241",
                tableHeader: "#303446",
                border: "#6272a4",
                borderSoft: "#44475a",
                text: "#f8f8f2",
                mutedText: "#bcb0ff",
                accent: "#bd93f9",
                accentText: "#282a36",
                accentSoft: "#c7b3ff",
                accentStrong: "#d8c2ff",
                link: "#bd93f9",
                danger: "#ff5555",
                dangerSoft: "#5a2a35",
                dangerStrong: "#ff79c6",
                rowAlt: "#303241",
                rowSelected: "#44475a"
            },
            "solarized-dark": {
                isDark: true,
                window: "#002b36",
                surface: "#073642",
                panel: "#002b36",
                panelRaised: "#073642",
                header: "#003845",
                tableHeader: "#073642",
                border: "#586e75",
                borderSoft: "#174652",
                text: "#eee8d5",
                mutedText: "#93a1a1",
                accent: "#268bd2",
                accentText: "#002b36",
                accentSoft: "#2aa198",
                accentStrong: "#40a5e8",
                link: "#2aa198",
                danger: "#dc322f",
                dangerSoft: "#4a2d2b",
                dangerStrong: "#ff6b5f",
                rowAlt: "#003845",
                rowSelected: "#073642"
            },
            "solarized-light": {
                isDark: false,
                window: "#fdf6e3",
                surface: "#f3ebd6",
                panel: "#fdf6e3",
                panelRaised: "#f5ecd3",
                header: "#f8f1da",
                tableHeader: "#f5ecd3",
                border: "#d9d1be",
                borderSoft: "#e7dfc6",
                text: "#586e75",
                mutedText: "#657b83",
                accent: "#268bd2",
                accentText: "#fdf6e3",
                accentSoft: "#859900",
                accentStrong: "#1d75b8",
                link: "#268bd2",
                danger: "#b42318",
                dangerSoft: "#f2d8cf",
                dangerStrong: "#8f1b12",
                rowAlt: "#f8f1da",
                rowSelected: "#e7dfc6"
            },
            "mint-light": {
                isDark: false,
                window: "#eef6f4",
                surface: "#ffffff",
                panel: "#eef6f4",
                panelRaised: "#ffffff",
                header: "#e6f1ef",
                tableHeader: "#e6f1ef",
                border: "#d4e5e1",
                borderSoft: "#e6f1ef",
                text: "#233138",
                mutedText: "#4b5b61",
                accent: "#2f8f83",
                accentText: "#eef6f4",
                accentSoft: "#6fc5b6",
                accentStrong: "#21796f",
                link: "#2f8f83",
                danger: "#b42318",
                dangerSoft: "#f8d7d2",
                dangerStrong: "#8f1b12",
                rowAlt: "#e6f1ef",
                rowSelected: "#d9f1ec"
            },
            "paper": {
                isDark: false,
                window: "#f5f2e9",
                surface: "#ffffff",
                panel: "#f5f2e9",
                panelRaised: "#ffffff",
                header: "#efe9dc",
                tableHeader: "#efe9dc",
                border: "#e0d7c7",
                borderSoft: "#efe9dc",
                text: "#3b3a36",
                mutedText: "#6f6559",
                accent: "#c07a3a",
                accentText: "#ffffff",
                accentSoft: "#d7b08a",
                accentStrong: "#9d5f24",
                link: "#b36b2f",
                danger: "#b42318",
                dangerSoft: "#efd7ce",
                dangerStrong: "#8f1b12",
                rowAlt: "#efe9dc",
                rowSelected: "#e6dccb"
            },
            "tokyo-night": {
                isDark: true,
                window: "#1a1b26",
                surface: "#16161e",
                panel: "#1f2335",
                panelRaised: "#24283b",
                header: "#1f2335",
                tableHeader: "#24283b",
                border: "#394260",
                borderSoft: "#2f354d",
                text: "#c0caf5",
                mutedText: "#9aa5d0",
                accent: "#7aa2f7",
                accentText: "#16161e",
                accentSoft: "#9fb4ff",
                accentStrong: "#a9c2ff",
                link: "#9fb4ff",
                danger: "#f7768e",
                dangerSoft: "#4f2d3a",
                dangerStrong: "#ff9bab",
                rowAlt: "#1f2335",
                rowSelected: "#24283b"
            },
            "catppuccin": {
                isDark: true,
                window: "#1e1e2e",
                surface: "#181825",
                panel: "#1e1e2e",
                panelRaised: "#313244",
                header: "#313244",
                tableHeader: "#313244",
                border: "#585b70",
                borderSoft: "#45475a",
                text: "#f5e0dc",
                mutedText: "#e8d7ff",
                accent: "#cba6f7",
                accentText: "#1e1e2e",
                accentSoft: "#f2d0ff",
                accentStrong: "#ddb6ff",
                link: "#cba6f7",
                danger: "#f38ba8",
                dangerSoft: "#513244",
                dangerStrong: "#ffb3c4",
                rowAlt: "#313244",
                rowSelected: "#45475a"
            },
            "nord": {
                isDark: true,
                window: "#2e3440",
                surface: "#3b4252",
                panel: "#2e3440",
                panelRaised: "#3b4252",
                header: "#303747",
                tableHeader: "#3b4252",
                border: "#4c566a",
                borderSoft: "#434c5e",
                text: "#e5e9f0",
                mutedText: "#d8dee9",
                accent: "#88c0d0",
                accentText: "#2e3440",
                accentSoft: "#81a1c1",
                accentStrong: "#9bd4e4",
                link: "#88c0d0",
                danger: "#bf616a",
                dangerSoft: "#4c3138",
                dangerStrong: "#d77a83",
                rowAlt: "#303747",
                rowSelected: "#3b4252"
            }
        })
    readonly property var palette: palettes[themeName] || palettes["classico"]
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

    readonly property var themeOptions: ["system", "classico", "mint-light", "paper", "solarized-light", "windows7", "catppuccin", "dark", "dracula", "grayscale", "gruvbox", "nord", "solarized-dark", "tokyo-night"]

    function densityValue(density, compactValue, normalValue, comfortableValue) {
        if (density === "normal") {
            return normalValue;
        }
        return density === "comfortable" ? comfortableValue : compactValue;
    }

    function bottomPaneHeight(windowHeight) {
        return Math.max(bottomPaneMinHeight, Math.min(bottomPaneMaxHeight, Math.round(windowHeight * bottomPaneHeightRatio)));
    }
}
