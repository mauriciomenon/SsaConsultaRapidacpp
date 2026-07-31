# Search Header, Runtime Build Info, and Theme Contrast

## Scope

This slice changes only the main search header, the application window title,
the About dialog, and foreground selection in the relation navigator. It does
not change filters, imports, database access, relation navigation, or applied
filter behavior.

The remaining whole-interface review is read-only. Findings outside the four
approved areas are reported for a later decision and are not fixed in this
slice.

## Main Search Header

The first row keeps the existing control order and behavior. The ISO week and
SSA count become text-only labels with no external background, border, or
padding. Both use the 14 px title size with the existing light bold weight.

The horizontal gaps are explicit:

- 20 px from the end of the general search group to the ISO week text.
- 10 px from the ISO week text to the SSA count.
- 20 px from the SSA count to the Importar XLSX button.
- Existing compact spacing remains between Importar XLSX, Preferencias, and
  the theme button.

No button handler, search behavior, or filter state changes.

## Window Title

The main title is assembled from runtime values:

`Consulta Rapida de SSAs v<application-version> - <ISO-year-week>`

The ISO value comes from `CurrentWeekViewModel.value`, which already exposes
the six-digit ISO year/week value and updates when the week changes.

## Relation Contrast

The reproduced Dracula failure is caused by a non-selected child node using
the light `accentSoft` background with the light default text. The current
conditional only handles the selected node.

Every text element inside a relation node will derive its foreground from the
node's effective background through `Theme.readableText`. Semantic preferred
colors remain hints, but WCAG AA contrast against the actual rendered fill is
authoritative. Navigation behavior and node fills remain unchanged.

## About Dialog

The dialog shows only:

- Product name.
- Running application version.
- Actual compiler family and version for the running binary.
- Close button.

Static validation history, platform claims, unsupported-toolchain text, and
the author line are removed. Compiler text is determined at compile time from
MSVC, Clang, or GCC macros and passed to QML as immutable startup data.

## Validation

Tests are written and observed failing before production changes. Focused
contracts cover the 14 px text-only header, exact gaps, dynamic title, concise
About content, compiler text, and effective relation-node contrast across all
39 named palettes.

After static review and focused compilation, the Windows amd64 application is
opened and inspected as a real native window. Offscreen measurements are only
preliminary evidence.

The final visual artifact set contains two named PNG files for every selectable
theme option, including the Windows-resolved `system` option:

- `<theme>-full.png`: complete main window.
- `<theme>-relations.png`: the relation navigator area referenced by the user.

The captures use the same window size, data, selected SSA, relation sequence,
and UI state. They are stored outside Git so the next round can compare,
correct, or remove themes without adding generated images to the repository.

## Reporting

The final report distinguishes working-tree changes, commit, local validation,
and remote push. It reports GitLab and Bitbucket commit refs only. GitLab CI
minute limitations and Bitbucket proximity warnings are omitted unless the
Bitbucket quota is actually reached.
