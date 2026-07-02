# Preferences Schema

## Version 1

Runtime file: `ssa_cpp_preferences.json`.

Current fields:

- `schema_version`: integer.
- `page_size`: integer from 10 to 500.
- `theme`: one of the values accepted by `UserPreferenceDefaults`, including `system`,
  `ssa-dark`, `light`, `dark`, and `gruvbox`.
- `density`: `compact`, `normal`, or `comfortable`.
- `details_visible`: boolean controlling the right details panel.
- `details_panel_width`: integer from 320 to 1200.
- `sort_column_key`: column key used by the current table sort.
- `sort_ascending`: boolean direction for the current table sort.
- `visible_columns`: ordered list of column keys.
- `column_widths`: object keyed by column key with integer widths.
- `quick_sector`: string used by the executor shortcut filter.
- `exclude_sca_ses_ste`: boolean for the `SCA/SES/STE` exclusion.
- `column_filters`: object keyed by column key with filter text.
- `advanced_text_filters`: object keyed by column key with advanced filter text.
- `advanced_week_column_key`: week column key used by advanced week/year filters.
- `advanced_year`: string year filter, empty when disabled.
- `advanced_week`: string week filter, empty when disabled.
- `issue_year`: string issue year filter over `semana_cadastro`, empty when disabled.
- `execution_year`: string execution year filter over `semana_executada`, empty when disabled.
- `reprogramming_equals`: string exact reprogramming count, empty when disabled.
- `issue_week_start`: string lower bound for issue `AnoSemana`, empty when disabled.
- `issue_week_end`: string upper bound for issue `AnoSemana`, empty when disabled.
- `execution_week_start`: string lower bound for execution `AnoSemana`, empty when disabled.
- `execution_week_end`: string upper bound for execution `AnoSemana`, empty when disabled.
- `derivation_mode`: `all`, `root`, or `derived`.
- `only_reprogrammed`: boolean for reprogrammed-only filter.

## Rules

- Code defaults are the source of fallback.
- Runtime preferences override defaults only when valid.
- Invalid files must fail clearly at the persistence boundary.
- Column widths are clamped by the presentation model to the supported UI range.
- Visible column preferences must be reconciled against `ColumnCatalog`.
- Column filter preferences must be reconciled against `ColumnCatalog`.
- Advanced text filter preferences must be reconciled against `ColumnCatalog`.
- Sort column preferences must be reconciled against `ColumnCatalog`.
- Advanced week column preferences must be reconciled against `ColumnCatalog`.
- `system` theme is persisted as a stable value and follows the platform color scheme when Qt
  exposes it.
- Density changes table row height, table header height, and detail panel text sizing only.
- Detail panel width is clamped by UI policy in presentation and enforced by both persistence load and presentation.
- Empty `visible_columns` is treated as invalid and falls back to defaults because the table
  requires at least one visible column.
- Future migrations require an ADR.
