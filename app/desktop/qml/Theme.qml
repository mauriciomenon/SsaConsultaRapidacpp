pragma Singleton

import QtQuick

QtObject {
    property string themeName: "ssa-dark"

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
                accent: "#9fb8c8",
                accentText: "#1c1c1c",
                accentSoft: "#cfd3d6",
                accentStrong: "#ffffff",
                link: "#8fb4c8",
                danger: "#ff9a9a",
                dangerSoft: "#4a2d2d",
                dangerStrong: "#ffb8b8",
                rowAlt: "#31343a",
                rowSelected: "#3a3d44"
            },
            "windows7": {
                isDark: false,
                window: "#e6edf7",
                surface: "#f1f5fb",
                panel: "#eaeff7",
                panelRaised: "#f4f7fc",
                header: "#dde6f2",
                tableHeader: "#d3e0f0",
                border: "#c8d4e6",
                borderSoft: "#dde6f2",
                text: "#1a1a1a",
                mutedText: "#3c4a6f",
                accent: "#1f56a0",
                accentText: "#f4f7fc",
                accentSoft: "#cfe0f2",
                accentStrong: "#16437a",
                link: "#1f56a0",
                danger: "#b42318",
                dangerSoft: "#f0d6d2",
                dangerStrong: "#8f1b12",
                rowAlt: "#eef3fa",
                rowSelected: "#cfe0f2"
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
                accent: "#1f66c2",
                accentText: "#ffffff",
                accentSoft: "#d9ecff",
                accentStrong: "#1550a0",
                link: "#1f66c2",
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
                link: "#fecc55",
                danger: "#ff7d6d",
                dangerSoft: "#3a2220",
                dangerStrong: "#ffa090",
                rowAlt: "#32302f",
                rowSelected: "#504945"
            },
            "ssa-dark": {
                isDark: true,
                window: "#222936",
                surface: "#27303d",
                panel: "#2e3542",
                panelRaised: "#384252",
                header: "#2b3442",
                tableHeader: "#323b49",
                border: "#596577",
                borderSoft: "#465264",
                text: "#edf2f8",
                mutedText: "#c6d0dc",
                accent: "#8fc9d8",
                accentText: "#18202b",
                accentSoft: "#445f6c",
                accentStrong: "#a8dce8",
                link: "#a8dce8",
                danger: "#e58f8f",
                dangerSoft: "#4a3038",
                dangerStrong: "#ffb0b0",
                rowAlt: "#29323f",
                rowSelected: "#465a66"
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
                accent: "#7aa2c8",
                accentText: "#ffffff",
                accentSoft: "#9c9c9c",
                accentStrong: "#a9c8e8",
                link: "#9bb8d4",
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
                danger: "#ff6e6e",
                dangerSoft: "#5a2a35",
                dangerStrong: "#ff8a80",
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
                accent: "#3aa0e8",
                accentText: "#002b36",
                accentSoft: "#1c5e57",
                accentStrong: "#5cb5f0",
                link: "#2aa198",
                danger: "#ff6b5f",
                dangerSoft: "#3a1f1d",
                dangerStrong: "#ff8a7d",
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
                mutedText: "#4f6373",
                accent: "#1d6fb8",
                accentText: "#fdf6e3",
                accentSoft: "#e8efc4",
                accentStrong: "#155a99",
                link: "#1d6fb8",
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
                accent: "#21796f",
                accentText: "#eef6f4",
                accentSoft: "#6fc5b6",
                accentStrong: "#185e56",
                link: "#21796f",
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
                accent: "#9d5f24",
                accentText: "#ffffff",
                accentSoft: "#d7b08a",
                accentStrong: "#7a4314",
                link: "#8a4f18",
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
                danger: "#d77a83",
                dangerSoft: "#3a2327",
                dangerStrong: "#ec9aa2",
                rowAlt: "#303747",
                rowSelected: "#3b4252"
            },
            // Python theme imports (ipsis litteris from utils/themes.py THEME_ROLES).
            // Suffix "py" to distinguish from the C++ originals until refined.
            // Mapping: accent→accent, accent_soft→accentSoft, label_color→text,
            // support_text_color→mutedText, panel_bg→panel, panel_border→border,
            // input_bg→surface, input_border_focus→accentStrong, tag_pressed→panelRaised.
            // Roles without Python source (danger*, link, header, tableHeader,
            // borderSoft, rowAlt, rowSelected, accentText) are derived to keep
            // the UI functional.
            "grayscalepy": {
                isDark: true,
                window: "#2b2e33",
                surface: "#2f3238",
                panel: "#2b2e33",
                panelRaised: "#3a3d44",
                header: "#2b2e33",
                tableHeader: "#2b2e33",
                border: "#4a4d52",
                borderSoft: "#3a3d44",
                text: "#f0f0f0",
                mutedText: "#cdd0d4",
                accent: "#e6e6e6",
                accentText: "#1c1c1c",
                accentSoft: "#cfd3d6",
                accentStrong: "#e6e6e6",
                link: "#cfd3d6",
                danger: "#ff9a9a",
                dangerSoft: "#4a2d2d",
                dangerStrong: "#ffb8b8",
                rowAlt: "#31343a",
                rowSelected: "#3a3d44"
            },
            "windows7py": {
                isDark: false,
                window: "#f8fbff",
                surface: "#ffffff",
                panel: "#f8fbff",
                panelRaised: "#ffffff",
                header: "#f8fbff",
                tableHeader: "#f8fbff",
                border: "#d8e5fb",
                borderSoft: "#e8f0fd",
                text: "#1a1a1a",
                mutedText: "#3c4a6f",
                accent: "#2b6cc0",
                accentText: "#ffffff",
                accentSoft: "#4b76c2",
                accentStrong: "#2b6cc0",
                link: "#2b6cc0",
                danger: "#b42318",
                dangerSoft: "#f0d6d2",
                dangerStrong: "#8f1b12",
                rowAlt: "#f0f7ff",
                rowSelected: "#d9ecff"
            },
            "classicopy": {
                isDark: false,
                window: "#f6f5f4",
                surface: "#fdfdfd",
                panel: "#f6f5f4",
                panelRaised: "#fdfdfd",
                header: "#f6f5f4",
                tableHeader: "#f6f5f4",
                border: "#d7d4d1",
                borderSoft: "#e6e3df",
                text: "#2e3436",
                mutedText: "#4c5153",
                accent: "#3584e4",
                accentText: "#ffffff",
                accentSoft: "#5a96e9",
                accentStrong: "#3584e4",
                link: "#3584e4",
                danger: "#b42318",
                dangerSoft: "#f8d7d2",
                dangerStrong: "#8f1b12",
                rowAlt: "#f0efed",
                rowSelected: "#d9ecff"
            },
            "gruvboxpy": {
                isDark: true,
                window: "#3c3836",
                surface: "#3c3836",
                panel: "#3c3836",
                panelRaised: "#504945",
                header: "#3c3836",
                tableHeader: "#3c3836",
                border: "#665c54",
                borderSoft: "#504945",
                text: "#ebdbb2",
                mutedText: "#d5c4a1",
                accent: "#fabd2f",
                accentText: "#282828",
                accentSoft: "#ebdbb2",
                accentStrong: "#fabd2f",
                link: "#fecc55",
                danger: "#ff7d6d",
                dangerSoft: "#3a2220",
                dangerStrong: "#ffa090",
                rowAlt: "#32302f",
                rowSelected: "#504945"
            },
            "darkpy": {
                isDark: true,
                window: "#2a2a2a",
                surface: "#2b2b2b",
                panel: "#2a2a2a",
                panelRaised: "#333333",
                header: "#2a2a2a",
                tableHeader: "#2a2a2a",
                border: "#4d4d4d",
                borderSoft: "#3f3f3f",
                text: "#f0f0f0",
                mutedText: "#c2c2c2",
                accent: "#7a7a7a",
                accentText: "#ffffff",
                accentSoft: "#9c9c9c",
                accentStrong: "#4a90e2",
                link: "#9bb8d4",
                danger: "#ff8a80",
                dangerSoft: "#552b31",
                dangerStrong: "#ff8181",
                rowAlt: "#252525",
                rowSelected: "#3f3f3f"
            },
            "draculapy": {
                isDark: true,
                window: "#303446",
                surface: "#303446",
                panel: "#303446",
                panelRaised: "#44475a",
                header: "#303446",
                tableHeader: "#303446",
                border: "#6272a4",
                borderSoft: "#44475a",
                text: "#f8f8f2",
                mutedText: "#bcb0ff",
                accent: "#bd93f9",
                accentText: "#282a36",
                accentSoft: "#c7b3ff",
                accentStrong: "#bd93f9",
                link: "#bd93f9",
                danger: "#ff6e6e",
                dangerSoft: "#5a2a35",
                dangerStrong: "#ff8a80",
                rowAlt: "#303241",
                rowSelected: "#44475a"
            },
            "solarized-darkpy": {
                isDark: true,
                window: "#002b36",
                surface: "#073642",
                panel: "#002b36",
                panelRaised: "#073642",
                header: "#002b36",
                tableHeader: "#073642",
                border: "#586e75",
                borderSoft: "#174652",
                text: "#eee8d5",
                mutedText: "#93a1a1",
                accent: "#268bd2",
                accentText: "#002b36",
                accentSoft: "#93a1a1",
                accentStrong: "#2aa198",
                link: "#2aa198",
                danger: "#ff6b5f",
                dangerSoft: "#3a1f1d",
                dangerStrong: "#ff8a7d",
                rowAlt: "#003845",
                rowSelected: "#073642"
            },
            "solarized-lightpy": {
                isDark: false,
                window: "#fdf6e3",
                surface: "#f5ecd3",
                panel: "#fdf6e3",
                panelRaised: "#f5ecd3",
                header: "#fdf6e3",
                tableHeader: "#f5ecd3",
                border: "#d9d1be",
                borderSoft: "#e7dfc6",
                text: "#586e75",
                mutedText: "#657b83",
                accent: "#268bd2",
                accentText: "#fdf6e3",
                accentSoft: "#859900",
                accentStrong: "#268bd2",
                link: "#268bd2",
                danger: "#b42318",
                dangerSoft: "#f2d8cf",
                dangerStrong: "#8f1b12",
                rowAlt: "#f8f1da",
                rowSelected: "#e7dfc6"
            },
            "mint-lightpy": {
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
                accentStrong: "#185e56",
                link: "#21796f",
                danger: "#b42318",
                dangerSoft: "#f8d7d2",
                dangerStrong: "#8f1b12",
                rowAlt: "#e6f1ef",
                rowSelected: "#d9f1ec"
            },
            "paperpy": {
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
                accentStrong: "#7a4314",
                link: "#8a4f18",
                danger: "#b42318",
                dangerSoft: "#efd7ce",
                dangerStrong: "#8f1b12",
                rowAlt: "#efe9dc",
                rowSelected: "#e6dccb"
            },
            "tokyo-nightpy": {
                isDark: true,
                window: "#1f2335",
                surface: "#1f2335",
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
                accentStrong: "#7aa2f7",
                link: "#9fb4ff",
                danger: "#f7768e",
                dangerSoft: "#4f2d3a",
                dangerStrong: "#ff9bab",
                rowAlt: "#1f2335",
                rowSelected: "#24283b"
            },
            "catppuccinpy": {
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
            "nordpy": {
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
                danger: "#d77a83",
                dangerSoft: "#3a2327",
                dangerStrong: "#ec9aa2",
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
    readonly property string fontFamily: Qt.platform.os === "windows" ? "Segoe UI" : Qt.platform.os === "linux" ? "DejaVu Sans" : "Arial"
    readonly property int radius: 4
    readonly property int radiusSoft: 6
    readonly property int gap: 8
    readonly property int cardGap: 10
    readonly property int controlHeight: 30
    readonly property int detailsLabelWidth: 190
    readonly property int relationNodeMinWidth: 92
    readonly property int relationNodeHeight: 40
    readonly property int bottomPaneMinHeight: 320
    readonly property int bottomPaneMaxHeight: 430
    readonly property real bottomPaneHeightRatio: 0.4

    // ---------------------------------------------------------------------------
    // TypeScale - modular scale (base 12px, ratio 8:9 = 1.125, data-dense UI).
    // Every font.pixelSize in the app must reference one of these. No literal
    // font sizes in component bodies.
    // Base 12px is intentional for the dense SSA table UI. Future migration to
    // font.pointSize (OS DPI-aware) is tracked in RECOVERY_BACKLOG.md.
    // ---------------------------------------------------------------------------
    readonly property int fontSizeCaption: 10 // ms(-2) - badges, micro metadata
    readonly property int fontSizeMicro: 11 // ms(-1) - secondary value labels
    readonly property int fontSizeBody: 12 // ms(0) - body, inputs, base
    readonly property int fontSizeLabel: 13 // ms(1) - primary field labels
    readonly property int fontSizeTitle: 14 // ms(2) - card/section titles
    readonly property int fontSizeHeader: 16 // ms(3) - header/title text

    // ---------------------------------------------------------------------------
    // SpacingScale - every margin/padding/spacing references these. Replaces
    // the ~16 distinct literals (0..26) that were scattered across components.
    // ---------------------------------------------------------------------------
    readonly property int spacingSm: 4
    readonly property int spacingMd: gap // 8 - alias of the canonical gap

    // ---------------------------------------------------------------------------
    // ControlHeights - unified row/control heights. Ends the 26 vs 28 vs 24
    // misalignment between filter cards placed side by side in the grid.
    // ---------------------------------------------------------------------------
    readonly property int filterRowHeight: controlHeight - 2 // 28 - all filter cards
    readonly property int chipHeight: 26 // summary tag at normal density
    readonly property int chipHeightCompact: 24 // summary tag when >= 2 chips
    readonly property int statusShortcutHeight: 24 // status shortcut button

    // ---------------------------------------------------------------------------
    // PopupMetrics - widths/heights/margins for value popups.
    // Widths are DATA-DRIVEN: measured from real max field widths in the SSA
    // database (run measurement via sqlite SELECT MAX(LENGTH(col)) before
    // changing these numbers). Three categories by data type, not by column:
    //   - code: short fixed codes (setor/situacao/localizacao/status), max 15 chars
    //   - name: long person names (solicitante/responsavel_*), max 43 chars
    //   - anomalia: code + long description, up to 78 chars (own category)
    // The "code" and "name" widths are NOT fixed here; callers derive them
    // from the trigger control width (combo Valor) or the card width so the
    // popup matches its source. Only anomalia (which needs a fixed minimum to
    // show the full sentence) has a fixed width here.
    // ---------------------------------------------------------------------------
    readonly property int popupMargin: 8 // edge margin when clamping to window
    readonly property int popupAnomaliaWidth: 520 // anomalia (up to 78 chars) - fixed minimum
    // Upper bound for standard code/name popups. The actual width is resolved
    // from the runtime maximum value length by valuePopupWidth().
    readonly property int popupNameValueWidth: Math.min(popupAnomaliaWidth, Math.max(popupMultiSelectMinWidth, 43 * popupGlyphWidthPx + popupMultiSelectMinWidth - 90))
    readonly property int popupMaxHeight: 360 // hard cap so popups fit small windows
    readonly property int popupPreferredHeight: 360 // preferred multi-select height
    readonly property int popupMinContentHeight: 120 // min height for list popups (multi-select)
    // Average glyph width for fontSizeMicro (11px). Measured from the SSA
    // data: a 43-char name renders ~284px wide, so ~6.6px/char at this font.
    readonly property real popupGlyphWidthPx: 6.6
    // Minimum width of the multi-select popup so its fixed content always fits:
    // value label (~90) + Incluir column (choiceColumnWidth) + Excluir column
    // (choiceColumnWidth) + spacing (3 gaps) + popup padding (2*10).
    readonly property int popupMultiSelectMinWidth: 90 + choiceColumnWidth * 2 + 8 * 3 + 10 * 2
    // Limpar and Aplicar action buttons in the multi-select popup share one
    // width so the header stays balanced.
    readonly property int popupActionButtonWidth: 64

    // ---------------------------------------------------------------------------
    // FilterMetrics - geometry of the advanced filter cards. Replaces literals
    // like choiceColumnWidth: 52, commandWidth: 30, operatorWidth: 32.
    // ---------------------------------------------------------------------------
    // Grid layout of the advanced filter cards. resolveGridLayout is the
    // SINGLE source of truth: given the available width it returns how many
    // columns fit, the gap between cards, and the exact card width that fills
    // the row with no trailing empty space. AdvancedTextFilterGrid consumes
    // it and never recomputes columns/spacing/cardWidth on its own.
    readonly property int filterCardMinSpacing: 16 // gap at narrowest layout
    readonly property int filterCardMaxSpacing: 100 // gap at widest layout
    readonly property int filterCardMinWidth: 180 // floor so internal buttons fit
    readonly property int filterCardMaxColumns: 5

    // Resolve grid geometry: pick the most columns N that fit with each card
    // at least filterCardMinWidth (and at most filterCardMaxColumns), then
    // derive the exact cardWidth so N*cardWidth + (N-1)*spacing == width.
    // Spacing scales with the total width (2%), clamped to [min,max].
    function resolveGridLayout(availableWidth) {
        const spacing = Math.max(filterCardMinSpacing, Math.min(filterCardMaxSpacing, Math.round(availableWidth * 0.02)));
        let columns = 1;
        for (let n = filterCardMaxColumns; n >= 1; --n) {
            const w = Math.floor((availableWidth - (n - 1) * spacing) / n);
            if (w >= filterCardMinWidth) {
                columns = n;
                break;
            }
        }
        const cardWidth = Math.floor((availableWidth - (columns - 1) * spacing) / columns);
        return ({
                columns: columns,
                spacing: spacing,
                cardWidth: cardWidth
            });
    }
    readonly property int choiceColumnWidth: 52 // Incluir/Excluir checkbox column
    readonly property int commandWidth: 30 // "..." action button width
    readonly property int operatorWidth: 32 // operator combo (=, !=)
    readonly property int operatorPopupWidth: 96 // operator combo popup width
    readonly property int valueMinWidth: 82 // value combo minimum width
    readonly property int valuePreferredWidth: 96 // value combo preferred floor
    readonly property real valuePreferredRatio: 0.32 // value combo share of cardWidth
    readonly property int applyButtonWidth: 88 // "Aplicar" button in popups
    readonly property int weekFieldWidth: 72 // week/year field in week filter cards

    // ---------------------------------------------------------------------------
    // ChipMetrics - summary tag sizing. chipTextFactor/chipChromePadding back
    // the legacy JS estimator and are removed once Slice 6 lands the symmetric
    // QML implicit-width algorithm.
    // ---------------------------------------------------------------------------
    readonly property int chipChromePadding: 26 // label+button chrome allowance in SummaryTag
    readonly property int chipRemoveButtonSize: 24
    readonly property int chipRemoveButtonSizeCompact: 22
    readonly property int chipLabelMargin: 8
    readonly property int chipRemoveTooltipTimeoutMs: 10000

    // ---------------------------------------------------------------------------
    // FilterSummaryBarMetrics - active-filter summary bar geometry.
    // ---------------------------------------------------------------------------
    readonly property int summaryMinWidth: 220 // minimum scroll area for the tag row
    readonly property int summaryClearButtonOffset: 12 // clearance after the clear button
    readonly property int summaryTagSpacing: 6 // spacing between summary tags
    readonly property int summaryClearButtonWidth: 46
    readonly property int summaryLeftMargin: 4
    // Frame border: normal vs thicker when any exclusion (!) is active.
    // Border is drawn inside the bar bounds so status-shortcut spacing is untouched.
    readonly property int summaryBorderWidth: 1
    readonly property int summaryBorderWidthExcluded: 2

    // ---------------------------------------------------------------------------
    // WindowMetrics - clamp windows/dialogs to the available desktop area so
    // they never overflow small laptop screens (e.g. 1366x768).
    // ---------------------------------------------------------------------------
    readonly property int windowEdgeMargin: 24 // clearance from desktop edge
    readonly property int detailsWindowPreferredWidth: 885
    readonly property int detailsWindowPreferredHeight: 900
    readonly property int detailsWindowMinWidth: 880
    readonly property int detailsWindowMinHeight: 700
    readonly property int themeDialogPreferredWidth: 560
    readonly property int themeDialogPreferredHeight: 460
    readonly property int themeDialogMinWidth: 520
    readonly property int themeDialogMinHeight: 420

    // Clamp a preferred dimension to the available desktop area, leaving room
    // for windowEdgeMargin on both sides.
    function clampedWindowDimension(available, preferred, minimum) {
        const capped = Math.min(preferred, available - windowEdgeMargin * 2);
        return Math.max(minimum, capped);
    }

    // ---------------------------------------------------------------------------
    // StatusShortcutMetrics - status shortcut strip geometry.
    // ---------------------------------------------------------------------------
    readonly property int shortcutSpacing: 3
    readonly property int shortcutGap: 2 // popup y offset below the strip

    readonly property var themeOptions: ["system", "ssa-dark", "classico", "mint-light", "paper", "solarized-light", "windows7", "catppuccin", "dark", "dracula", "grayscale", "gruvbox", "nord", "solarized-dark", "tokyo-night", "grayscalepy", "windows7py", "classicopy", "gruvboxpy", "darkpy", "draculapy", "solarized-darkpy", "solarized-lightpy", "mint-lightpy", "paperpy", "tokyo-nightpy", "catppuccinpy", "nordpy"]

    function densityValue(density, compactValue, normalValue, comfortableValue) {
        if (density === "normal") {
            return normalValue;
        }
        return density === "comfortable" ? comfortableValue : compactValue;
    }

    // ---------------------------------------------------------------------------
    // Popup clamping helpers. Single source of truth for keeping popups inside
    // the window/overlay. All return values are ABSOLUTE overlay coordinates
    // (callers must parent the popup to Overlay.overlay). Replaces the 3
    // divergent implementations that lived in AdvancedTextFilterCard,
    // AppComboBox and AdvancedReprogrammingFilterCard.
    // Callers resolve their own attached Overlay/Window (only valid in the
    // component scope) and pass primitives here, so the singleton never touches
    // attached properties it does not own.
    // ---------------------------------------------------------------------------
    // Horizontal: align the popup's RIGHT edge with the trigger's RIGHT edge
    // (originRightX), shifting left only when it would overflow the window.
    function clampedPopupX(boundsWidth, originRightX, popupWidth) {
        const preferredX = originRightX - popupWidth;
        const leftLimit = popupMargin;
        const rightLimit = boundsWidth - popupWidth - popupMargin;
        return Math.max(leftLimit, Math.min(preferredX, rightLimit));
    }

    // Vertical: open directly below the trigger, clamped to the window.
    function clampedPopupY(boundsHeight, originY, originHeight, popupHeight) {
        const preferredY = originY + originHeight + shortcutGap;
        const bottomLimit = boundsHeight - popupHeight - popupMargin;
        const topLimit = popupMargin;
        return Math.max(topLimit, Math.min(preferredY, bottomLimit));
    }

    function clampedPopupHeight(boundsHeight, preferredHeight, minHeight) {
        const minH = minHeight !== undefined ? minHeight : popupMinContentHeight;
        return Math.max(minH, Math.min(preferredHeight, boundsHeight - popupMargin * 2));
    }

    // Height cap when the popup must open BELOW a trigger at originBottomY:
    // prefer opening below over clamping Y up. Falls back to preferredHeight
    // when there is plenty of room, otherwise shrinks to what fits below
    // (down to minHeight). Returns -1 if opening below cannot fit at all
    // (caller should then clamp Y up instead).
    function clampedPopupHeightBelow(boundsHeight, originBottomY, preferredHeight, minHeight) {
        const minH = minHeight !== undefined ? minHeight : popupMinContentHeight;
        const availableBelow = boundsHeight - originBottomY - popupMargin;
        if (availableBelow < minH)
            return -1;
        return Math.max(minH, Math.min(preferredHeight, availableBelow));
    }

    // Classify an advanced-filter column into a popup-width category based on
    // the data type it holds (measured from real max field widths). Categories:
    //   "code"     - short fixed codes (setor/situacao/localizacao/status)
    //   "name"     - long person names (solicitante/responsavel_*)
    //   "anomalia" - code + long description (own fixed width)
    // Single source of truth: add new columns here, not in each QML card.
    function valuePopupCategory(columnKey) {
        if (columnKey === "anomalia")
            return "anomalia";
        if (columnKey === "solicitante" || columnKey === "responsavel_programacao" || columnKey === "responsavel_execucao")
            return "name";
        return "code";
    }

    function valuePopupWidth(columnKey, maxValueLength, availableWidth) {
        const category = valuePopupCategory(columnKey);
        const normalizedLength = Math.max(0, Number(maxValueLength));
        const measuredWidth = Math.ceil(normalizedLength * popupGlyphWidthPx + popupMultiSelectMinWidth - 90);
        const categoryMaxWidth = category === "code" ? popupNameValueWidth : popupAnomaliaWidth;
        let resolvedWidth = Math.min(categoryMaxWidth, Math.max(popupMultiSelectMinWidth, measuredWidth));
        if (Number(availableWidth) > 0)
            resolvedWidth = Math.min(resolvedWidth, Math.max(0, Number(availableWidth) - popupMargin * 2));
        return Math.round(resolvedWidth);
    }

    // Relative luminance of a color (sRGB), clamped to [0,1]. Used to pick a
    // foreground that contrasts with a given background tint across themes.
    function luminance(color) {
        return (0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b);
    }

    // True when the background tint is dark enough that a light foreground
    // (Theme.text) reads better than a dark one (Theme.accentText).
    function isDarkTint(bg) {
        return luminance(bg) < 0.4;
    }

    function bottomPaneHeight(windowHeight) {
        return Math.max(bottomPaneMinHeight, Math.min(bottomPaneMaxHeight, Math.round(windowHeight * bottomPaneHeightRatio)));
    }
}
