pragma Singleton

import QtQuick

QtObject {
    property string themeName: "ssa-dark"

    readonly property bool gruvbox: themeName === "gruvbox"
    readonly property bool refinedNativeTheme: !themeName.endsWith("py")

    readonly property var palettes: ({
            "grayscale": {
                isDark: true,
                window: "#30343a",
                surface: "#363a41",
                panel: "#32363c",
                panelRaised: "#3d424a",
                header: "#373c43",
                tableHeader: "#3a3f46",
                border: "#59616a",
                borderSoft: "#474e56",
                text: "#e6e9ec",
                mutedText: "#c2c8ce",
                accent: "#94a8b5",
                accentText: "#172027",
                accentSoft: "#c0cbd2",
                accentStrong: "#aec0cb",
                link: "#a7bdc9",
                danger: "#d98989",
                dangerSoft: "#4d3438",
                dangerStrong: "#e7a1a1",
                rowAlt: "#363a41",
                rowSelected: "#48545d"
            },
            "windows7": {
                isDark: false,
                window: "#e4e9f0",
                surface: "#f0f3f7",
                panel: "#e9edf3",
                panelRaised: "#f4f6f9",
                header: "#dce3ec",
                tableHeader: "#d8e1eb",
                border: "#b9c5d3",
                borderSoft: "#cfd7e1",
                text: "#202934",
                mutedText: "#4d5b6d",
                accent: "#7d9cbd",
                accentText: "#162536",
                accentSoft: "#c1d0df",
                accentStrong: "#557a9f",
                link: "#416b95",
                danger: "#95504d",
                dangerSoft: "#ead1cf",
                dangerStrong: "#884743",
                rowAlt: "#e8edf3",
                rowSelected: "#ccd9e6"
            },
            "classico": {
                isDark: false,
                window: "#ecebea",
                surface: "#f5f4f2",
                panel: "#efeeec",
                panelRaised: "#f8f7f5",
                header: "#dedfdf",
                tableHeader: "#e7e5e2",
                border: "#c6c3bf",
                borderSoft: "#d8d5d1",
                text: "#2c3336",
                mutedText: "#555c5f",
                accent: "#88a7c5",
                accentText: "#172534",
                accentSoft: "#c6d5e4",
                accentStrong: "#5d82a6",
                link: "#39668f",
                danger: "#934d49",
                dangerSoft: "#ead2ce",
                dangerStrong: "#844742",
                rowAlt: "#e8e6e3",
                rowSelected: "#cedae5"
            },
            "gruvbox": {
                isDark: true,
                window: "#30302d",
                surface: "#343532",
                panel: "#393936",
                panelRaised: "#474640",
                header: "#353532",
                tableHeader: "#3e3e39",
                border: "#625f56",
                borderSoft: "#504e47",
                text: "#e2d8ba",
                mutedText: "#c8bda0",
                accent: "#c9ad65",
                accentText: "#27271f",
                accentSoft: "#d9c88f",
                accentStrong: "#dcc47e",
                link: "#d4bd75",
                danger: "#d68172",
                dangerSoft: "#4b3431",
                dangerStrong: "#e19b8d",
                rowAlt: "#353532",
                rowSelected: "#504b3e"
            },
            "ssa-dark": {
                isDark: true,
                window: "#28303c",
                surface: "#2d3743",
                panel: "#323b48",
                panelRaised: "#3d4857",
                header: "#303a47",
                tableHeader: "#35404d",
                border: "#596779",
                borderSoft: "#485669",
                text: "#e7edf3",
                mutedText: "#c5d0da",
                accent: "#86aeb8",
                accentText: "#17242a",
                accentSoft: "#b4cbd0",
                accentStrong: "#9fc1c8",
                link: "#9fc1c8",
                danger: "#cf8588",
                dangerSoft: "#4b353b",
                dangerStrong: "#e19a9d",
                rowAlt: "#2f3945",
                rowSelected: "#475b62"
            },
            "dark": {
                isDark: true,
                window: "#25282c",
                surface: "#2a2e33",
                panel: "#30353b",
                panelRaised: "#394047",
                header: "#2e3339",
                tableHeader: "#343a40",
                border: "#56606a",
                borderSoft: "#474f58",
                text: "#e8ebee",
                mutedText: "#bac1c7",
                accent: "#859caf",
                accentText: "#15212a",
                accentSoft: "#b7c2ca",
                accentStrong: "#a5b7c5",
                link: "#9fb2c1",
                danger: "#d88983",
                dangerSoft: "#4f353a",
                dangerStrong: "#e5a09b",
                rowAlt: "#2e3339",
                rowSelected: "#465057"
            },
            "dracula": {
                isDark: true,
                window: "#2d303b",
                surface: "#323540",
                panel: "#363a47",
                panelRaised: "#444956",
                header: "#343743",
                tableHeader: "#3a3e4b",
                border: "#5b637a",
                borderSoft: "#4b5264",
                text: "#ecebe7",
                mutedText: "#c3bdd6",
                accent: "#a99ac4",
                accentText: "#211f29",
                accentSoft: "#ccc3dc",
                accentStrong: "#c0b2d5",
                link: "#bcadd2",
                danger: "#d98282",
                dangerSoft: "#50353d",
                dangerStrong: "#e49a96",
                rowAlt: "#343743",
                rowSelected: "#4b4859"
            },
            "solarized-dark": {
                isDark: true,
                window: "#17343b",
                surface: "#1d3b42",
                panel: "#19373e",
                panelRaised: "#28474e",
                header: "#1e3d44",
                tableHeader: "#234249",
                border: "#536b70",
                borderSoft: "#36535a",
                text: "#e5dfcc",
                mutedText: "#b4bfbd",
                accent: "#6a9fbd",
                accentText: "#102b33",
                accentSoft: "#a8c1c3",
                accentStrong: "#86b3ca",
                link: "#7eb4ae",
                danger: "#d57f76",
                dangerSoft: "#493532",
                dangerStrong: "#df988e",
                rowAlt: "#1e3d44",
                rowSelected: "#365157"
            },
            "solarized-light": {
                isDark: false,
                window: "#eee8d7",
                surface: "#f5efdf",
                panel: "#f0ead9",
                panelRaised: "#f7f1e3",
                header: "#e8e2d1",
                tableHeader: "#ece5d4",
                border: "#c9c2b2",
                borderSoft: "#ddd5c3",
                text: "#4f6269",
                mutedText: "#516168",
                accent: "#87a6bd",
                accentText: "#16303d",
                accentSoft: "#c7d4d7",
                accentStrong: "#527e9c",
                link: "#356783",
                danger: "#955149",
                dangerSoft: "#e8d2cb",
                dangerStrong: "#854a43",
                rowAlt: "#ebe4d3",
                rowSelected: "#d4dcda"
            },
            "mint-light": {
                isDark: false,
                window: "#e5efec",
                surface: "#f1f6f4",
                panel: "#e9f1ef",
                panelRaised: "#f4f8f7",
                header: "#dfeae7",
                tableHeader: "#e3ecea",
                border: "#bdceca",
                borderSoft: "#d2dfdc",
                text: "#26363a",
                mutedText: "#516166",
                accent: "#7faea6",
                accentText: "#142b27",
                accentSoft: "#bdd7d2",
                accentStrong: "#4f8078",
                link: "#386f67",
                danger: "#945049",
                dangerSoft: "#ead2ce",
                dangerStrong: "#854842",
                rowAlt: "#e2ece9",
                rowSelected: "#cbded9"
            },
            "paper": {
                isDark: false,
                window: "#ebe7dc",
                surface: "#f4f1e9",
                panel: "#eeeade",
                panelRaised: "#f7f4ec",
                header: "#e2dccf",
                tableHeader: "#e7e1d5",
                border: "#c9c1b2",
                borderSoft: "#dbd4c7",
                text: "#3b3a36",
                mutedText: "#5d574f",
                accent: "#b09270",
                accentText: "#2b2118",
                accentSoft: "#d9c9b8",
                accentStrong: "#836346",
                link: "#735538",
                danger: "#945047",
                dangerSoft: "#e6d2cb",
                dangerStrong: "#844b43",
                rowAlt: "#e6e0d4",
                rowSelected: "#d8cfc0"
            },
            "tokyo-night": {
                isDark: true,
                window: "#242735",
                surface: "#292c3a",
                panel: "#2d3141",
                panelRaised: "#373c4f",
                header: "#2b2f3e",
                tableHeader: "#323648",
                border: "#4d5670",
                borderSoft: "#40475e",
                text: "#d8ddeb",
                mutedText: "#b6bed6",
                accent: "#8fa4cc",
                accentText: "#182031",
                accentSoft: "#bec9df",
                accentStrong: "#a7b8d9",
                link: "#a7b8d9",
                danger: "#d77f8c",
                dangerSoft: "#513842",
                dangerStrong: "#e398a2",
                rowAlt: "#2b2f3e",
                rowSelected: "#41495d"
            },
            "catppuccin": {
                isDark: true,
                window: "#2a2937",
                surface: "#2f2e3d",
                panel: "#333241",
                panelRaised: "#403f50",
                header: "#363545",
                tableHeader: "#3a3949",
                border: "#5c5a70",
                borderSoft: "#4b495d",
                text: "#e9e4e4",
                mutedText: "#c4bccf",
                accent: "#b09dbf",
                accentText: "#241f2a",
                accentSoft: "#d0c4d7",
                accentStrong: "#c2afd0",
                link: "#c2afd0",
                danger: "#d98798",
                dangerSoft: "#513842",
                dangerStrong: "#e4a0ad",
                rowAlt: "#363545",
                rowSelected: "#4b4758"
            },
            "nord": {
                isDark: true,
                window: "#343b47",
                surface: "#39414f",
                panel: "#363d49",
                panelRaised: "#444d5c",
                header: "#3a4250",
                tableHeader: "#3e4755",
                border: "#596477",
                borderSoft: "#4a5567",
                text: "#e6e9ee",
                mutedText: "#c8ced6",
                accent: "#8cabb5",
                accentText: "#1e2a30",
                accentSoft: "#b9cbd0",
                accentStrong: "#a6c1c8",
                link: "#a6c1c8",
                danger: "#c7848a",
                dangerSoft: "#4b353a",
                dangerStrong: "#d89da2",
                rowAlt: "#3a4250",
                rowSelected: "#4a5760"
            },
            // Tinted Base16 imports mapped to the semantic GUI roles by the
            // offline theme lab. Sources are pinned in THIRD_PARTY_NOTICES.md.
            "ayu-light": {
                isDark: false,
                window: "#edeff1",
                surface: "#f5f6f7",
                panel: "#f2f3f5",
                panelRaised: "#e3e4e7",
                header: "#edeef0",
                tableHeader: "#e7e9eb",
                border: "#a0a6ac",
                borderSoft: "#c8cbcf",
                text: "#5c6166",
                mutedText: "#60656b",
                accent: "#5499cb",
                accentText: "#000000",
                accentSoft: "#e1e8ef",
                accentStrong: "#5295c5",
                link: "#3d6f94",
                danger: "#de8383",
                dangerSoft: "#f1e7e8",
                dangerStrong: "#ca7777",
                rowAlt: "#f4f5f6",
                rowSelected: "#e5ebf1"
            },
            "ayu-mirage": {
                isDark: true,
                window: "#242936",
                surface: "#292d36",
                panel: "#272b36",
                panelRaised: "#2e333e",
                header: "#292e39",
                tableHeader: "#2c313c",
                border: "#4a5059",
                borderSoft: "#3c424c",
                text: "#edeceb",
                mutedText: "#ececed",
                accent: "#94c5de",
                accentText: "#000000",
                accentSoft: "#536d7c",
                accentStrong: "#94c5de",
                link: "#94c5de",
                danger: "#e0958b",
                dangerSoft: "#7a5555",
                dangerStrong: "#e0958b",
                rowAlt: "#282c36",
                rowSelected: "#546d7d"
            },
            "flexoki-dark": {
                isDark: true,
                window: "#2a2a29",
                surface: "#2f2e2f",
                panel: "#2d2c2c",
                panelRaised: "#313131",
                header: "#2e2e2e",
                tableHeader: "#303030",
                border: "#575653",
                borderSoft: "#454543",
                text: "#cecdc3",
                mutedText: "#c0bfbd",
                accent: "#4385be",
                accentText: "#000000",
                accentSoft: "#355371",
                accentStrong: "#4385be",
                link: "#6a96c5",
                danger: "#cd5045",
                dangerSoft: "#723935",
                dangerStrong: "#cd5045",
                rowAlt: "#2e2d2e",
                rowSelected: "#344f6a"
            },
            "flexoki-light": {
                isDark: false,
                window: "#f2f0e5",
                surface: "#faf7eb",
                panel: "#f6f4e8",
                panelRaised: "#efede1",
                header: "#f4f1e6",
                tableHeader: "#f1efe4",
                border: "#cecdc3",
                borderSoft: "#e1dfd4",
                text: "#403e3c",
                mutedText: "#676661",
                accent: "#2f5f97",
                accentText: "#ffffff",
                accentSoft: "#e3e3de",
                accentStrong: "#2f5f97",
                link: "#2f5f97",
                danger: "#ab342d",
                dangerSoft: "#eee3d8",
                dangerStrong: "#ab342d",
                rowAlt: "#f8f5ea",
                rowSelected: "#e8e7e0"
            },
            "kanagawa": {
                isDark: true,
                window: "#2c2c33",
                surface: "#313134",
                panel: "#2f2f34",
                panelRaised: "#2c3342",
                header: "#2e3139",
                tableHeader: "#2d323e",
                border: "#54546d",
                borderSoft: "#414459",
                text: "#dcd7ba",
                mutedText: "#d5d5d3",
                accent: "#7e9cd8",
                accentText: "#000000",
                accentSoft: "#4e5c7b",
                accentStrong: "#7e9cd8",
                link: "#7e9cd8",
                danger: "#9e3b3e",
                dangerSoft: "#6d3538",
                dangerStrong: "#c65759",
                rowAlt: "#303034",
                rowSelected: "#4d5a79"
            },
            "kanagawa-dragon": {
                isDark: true,
                window: "#2e2d2d",
                surface: "#323131",
                panel: "#302f2f",
                panelRaised: "#363534",
                header: "#323131",
                tableHeader: "#343333",
                border: "#625e5a",
                borderSoft: "#4d4b48",
                text: "#d5d8d5",
                mutedText: "#d6d7d6",
                accent: "#8ba4b0",
                accentText: "#000000",
                accentSoft: "#535e64",
                accentStrong: "#8ba4b0",
                link: "#8ba4b0",
                danger: "#c4746e",
                dangerSoft: "#6e4846",
                dangerStrong: "#c4746e",
                rowAlt: "#313030",
                rowSelected: "#535e64"
            },
            "rose-pine": {
                isDark: true,
                window: "#2c2b37",
                surface: "#302f36",
                panel: "#2e2d37",
                panelRaised: "#2b2938",
                header: "#2d2c37",
                tableHeader: "#2c2a38",
                border: "#6e6a86",
                borderSoft: "#524e65",
                text: "#e6e4f5",
                mutedText: "#e6e6ea",
                accent: "#c4a9e5",
                accentText: "#000000",
                accentSoft: "#6d6080",
                accentStrong: "#c4a9e5",
                link: "#c4a9e5",
                danger: "#dd7d98",
                dangerSoft: "#7a4c5b",
                dangerStrong: "#dd7d98",
                rowAlt: "#2f2e36",
                rowSelected: "#6e6080"
            },
            "rose-pine-moon": {
                isDark: true,
                window: "#2a283a",
                surface: "#2e2c42",
                panel: "#2c2a3e",
                panelRaised: "#35314b",
                header: "#2f2d43",
                tableHeader: "#322f48",
                border: "#6e6a86",
                borderSoft: "#54516b",
                text: "#e5e4f5",
                mutedText: "#e5e5e9",
                accent: "#c4a9e5",
                accentText: "#000000",
                accentSoft: "#6d5f83",
                accentStrong: "#c4a9e5",
                link: "#c4a9e5",
                danger: "#dd7d98",
                dangerSoft: "#7a4a60",
                dangerStrong: "#dd7d98",
                rowAlt: "#2d2b40",
                rowSelected: "#6d5f83"
            },
            "rose-pine-dawn": {
                isDark: false,
                window: "#faf4ed",
                surface: "#f4efe8",
                panel: "#f6f1ea",
                panelRaised: "#f3ece3",
                header: "#f5efe8",
                tableHeader: "#f4ede5",
                border: "#9893a5",
                borderSoft: "#d0caca",
                text: "#575279",
                mutedText: "#64617a",
                accent: "#907aa9",
                accentText: "#000000",
                accentSoft: "#e4dede",
                accentStrong: "#907aa9",
                link: "#7a678f",
                danger: "#b4637a",
                dangerSoft: "#eadeda",
                dangerStrong: "#b4637a",
                rowAlt: "#f5f0e9",
                rowSelected: "#e7e1e0"
            },
            "primer-dark": {
                isDark: true,
                window: "#282c32",
                surface: "#303132",
                panel: "#2c2f32",
                panelRaised: "#303438",
                header: "#2e3034",
                tableHeader: "#2f3237",
                border: "#484f58",
                borderSoft: "#3c4249",
                text: "#d7dbdf",
                mutedText: "#d9dbdd",
                accent: "#7fa9d8",
                accentText: "#000000",
                accentSoft: "#4d6078",
                accentStrong: "#7fa9d8",
                link: "#7fa9d8",
                danger: "#e19590",
                dangerSoft: "#7a5554",
                dangerStrong: "#e19590",
                rowAlt: "#2e3032",
                rowSelected: "#4d6078"
            },
            "primer-light": {
                isDark: false,
                window: "#e1e4e8",
                surface: "#f5f6f7",
                panel: "#eceef0",
                panelRaised: "#e2e5e8",
                header: "#e9ebed",
                tableHeader: "#e5e7ea",
                border: "#959da5",
                borderSoft: "#c4c8cd",
                text: "#2f363d",
                mutedText: "#444d56",
                accent: "#3369a6",
                accentText: "#ffffff",
                accentSoft: "#dfe3ea",
                accentStrong: "#3369a6",
                link: "#3266a2",
                danger: "#cd4451",
                dangerSoft: "#eee3e4",
                dangerStrong: "#cd4451",
                rowAlt: "#f1f2f4",
                rowSelected: "#e3e7ec"
            },
            "oxocarbon-light": {
                isDark: false,
                window: "#dde1e6",
                surface: "#f2f4f8",
                panel: "#e9ecf0",
                panelRaised: "#d7dce3",
                header: "#e3e6ec",
                tableHeader: "#dde1e7",
                border: "#a1acba",
                borderSoft: "#c1c8d2",
                text: "#525f70",
                mutedText: "#546172",
                accent: "#4773c6",
                accentText: "#ffffff",
                accentSoft: "#dde2ef",
                accentStrong: "#4773c6",
                link: "#3b61a7",
                danger: "#ecd2df",
                dangerSoft: "#f0e7ee",
                dangerStrong: "#e1d6de",
                rowAlt: "#eef0f4",
                rowSelected: "#e1e5f1"
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

    readonly property var themeOptions: ["system", "ssa-dark", "classico", "mint-light", "paper", "solarized-light", "windows7", "catppuccin", "dark", "dracula", "grayscale", "gruvbox", "nord", "solarized-dark", "tokyo-night", "ayu-light", "ayu-mirage", "flexoki-dark", "flexoki-light", "kanagawa", "kanagawa-dragon", "rose-pine", "rose-pine-moon", "rose-pine-dawn", "primer-dark", "primer-light", "oxocarbon-light", "grayscalepy", "windows7py", "classicopy", "gruvboxpy", "darkpy", "draculapy", "solarized-darkpy", "solarized-lightpy", "mint-lightpy", "paperpy", "tokyo-nightpy", "catppuccinpy", "nordpy"]

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

    function wcagLuminance(color) {
        function linearChannel(channel) {
            return channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4);
        }
        return 0.2126 * linearChannel(color.r) + 0.7152 * linearChannel(color.g) + 0.0722 * linearChannel(color.b);
    }

    function contrastRatio(first, second) {
        const firstLuminance = wcagLuminance(first);
        const secondLuminance = wcagLuminance(second);
        return (Math.max(firstLuminance, secondLuminance) + 0.05) / (Math.min(firstLuminance, secondLuminance) + 0.05);
    }

    function readableText(background, preferred) {
        if (preferred !== undefined && contrastRatio(preferred, background) >= 4.5)
            return preferred;
        const candidates = [text, accentText, panelRaised];
        let best = candidates[0];
        let bestContrast = contrastRatio(best, background);
        for (let index = 1; index < candidates.length; ++index) {
            const candidateContrast = contrastRatio(candidates[index], background);
            if (candidateContrast > bestContrast) {
                best = candidates[index];
                bestContrast = candidateContrast;
            }
        }
        if (bestContrast >= 4.5)
            return best;
        const black = Qt.color("#000000");
        const white = Qt.color("#ffffff");
        return contrastRatio(black, background) >= contrastRatio(white, background) ? black : white;
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
