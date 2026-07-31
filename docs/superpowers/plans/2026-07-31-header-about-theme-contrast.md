# Header, About, and Theme Contrast Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Improve the main header, runtime title and About dialog, fix relation-node contrast, and produce native Windows comparison captures for every theme.

**Architecture:** Keep layout changes in QML, expose immutable compiler information from the desktop startup boundary, and use the existing `Theme.readableText` contract against actual rendered backgrounds. Smoke-only capture support remains outside product behavior.

**Tech Stack:** C++20, Qt 6, QML, QtTest, CMake, Windows MSVC amd64.

## Global Constraints

- Work directly on the current `master`; do not create a branch or worktree.
- Do not change filters, imports, database contracts, or applied-filter behavior.
- Use 14 px for ISO week and SSA count.
- Use exact header gaps of 20 px, 10 px, and 20 px.
- Store generated PNG files outside Git.

---

### Task 1: Header and Runtime Title

**Files:**
- Modify: `tests/smoke/AdvancedPopupQmlTest.cpp`
- Modify: `app/desktop/qml/components/SearchAndPager.qml`
- Modify: `app/desktop/qml/Main.qml`

**Interfaces:**
- Consumes: `CurrentWeekViewModel.value`, `Qt.application.version`.
- Produces: text-only 14 px labels and `Consulta Rapida de SSAs v<version> - <ISO week>`.

- [x] Add a QML smoke assertion that the week and count labels have no background, both use `Theme.fontSizeTitle`, and their scene gaps are 20/10/20 px.
- [x] Run only the new case and confirm RED against the boxed 13/11 px controls.
- [x] Set row spacing to zero, apply explicit left margins, remove label backgrounds/padding, and use `Theme.fontSizeTitle`.
- [x] Bind the main window title to runtime version and ISO week.
- [x] Run the focused case and confirm GREEN.

### Task 2: Concise Runtime About Information

**Files:**
- Modify: `tests/smoke/HelpAboutQmlTest.cpp`
- Modify: `app/desktop/DesktopApplicationRuntime.cpp`
- Modify: `app/desktop/qml/Main.qml`
- Modify: `app/desktop/qml/components/AboutDialog.qml`

**Interfaces:**
- Consumes: compiler macros `_MSC_VER`, `__clang_*__`, or `__GNUC__`.
- Produces: immutable `buildCompiler` startup property and concise About text.

- [x] Change the About smoke expectation to product name, application version, actual compiler text, and no validation/history/author copy.
- [x] Run the focused case and confirm RED.
- [x] Derive compiler family/version once in `DesktopApplicationRuntime.cpp` and pass it as an initial QML property.
- [x] Forward the value from `Main.qml` to `AboutDialog.qml`; remove static platform claims and author line.
- [x] Run the focused case and confirm GREEN.

### Task 3: Relation Navigator Contrast

**Files:**
- Modify: `tests/smoke/AdvancedPopupQmlTest.cpp`
- Modify: `app/desktop/qml/components/DetailsRelationNavigator.qml`

**Interfaces:**
- Consumes: the effective relation-node fill and `Theme.readableText(background, preferred)`.
- Produces: AA-readable badge, SSA number, and status text for every node state in all 39 palettes.

- [x] Add object names for the three node text elements and a 39-palette runtime contrast loop using parent, current, child, and related nodes.
- [x] Run the new case and confirm RED for the Dracula non-selected child.
- [x] Resolve each node foreground from `relationBox.color`, retaining the semantic preferred color only as a hint.
- [x] Run the focused case and confirm GREEN across all palettes.

### Task 4: Review and Focused Build

**Files:** all production and test files changed above.

- [x] Run automatic review before compilation: diff check, qmlformat, qmllint, clang-format, Semgrep, Gitleaks, detect-secrets, and TruffleHog on changed files.
- [x] Announce review findings before any correction; apply only in-scope fixes.
- [x] Build the focused Linux targets and run the affected QML suites.
- [x] Build Windows MSVC amd64 through the canonical Windows script and open the executable.

### Task 5: Native Per-Theme Visual Evidence

**Files:** generated artifacts only, outside Git.

- [x] Prepare one controlled database and relation state used by every capture.
- [x] For each of the 39 named palettes plus the Windows-resolved `system` option, open the native Windows application at the same size and state.
- [x] Save `<theme>-full.png` and `<theme>-relations.png` with the theme name visible in the artifact name.
- [x] Build contact sheets and inspect full-window alignment, button sizing, placement, and relation contrast.
- [x] Report read-only findings without changing additional themes in this slice.
- [x] Commit the implementation atomically, push to GitLab and Bitbucket, and verify both refs.

## Execution Evidence

- Native Windows captures: `C:/Users/mauri/AppData/Local/Temp/ssa-theme-audit-20260731.7C41gk/screens/`.
- Contact sheets: `C:/Users/mauri/AppData/Local/Temp/ssa-theme-audit-20260731.7C41gk/{full,relation}-themes-01..04.png`.
- Runtime title: `Consulta Rapida de SSAs v0.9.16 - 202631`.
- Runtime About: `Versao 0.9.16` and `MSVC 19.51` only.
- Implementation commit: `3c16e3c5dbbec63d3f1a95e4a2aabfc9c57e33a4`, verified on GitLab and Bitbucket.
- Visual audit: all 40 relation captures are readable; the Python-derived themes remain visibly more saturated and less refined than the native palettes and are deferred to the requested theme-removal round.
- Known pre-existing layout gate: `ssa_qml_layout_smoke_1180x760` still reports the clear/backspace glyph at 16.97 px inside 12 px of available width. This slice did not change that control.
