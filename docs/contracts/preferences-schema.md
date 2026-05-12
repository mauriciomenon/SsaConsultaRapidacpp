# Preferences Schema

## Version 1

Runtime file: `ssa_cpp_preferences.json`.

Current fields:

- `schema_version`: integer.
- `page_size`: integer from 10 to 500.
- `theme`: `system`, `light`, or `dark`.
- `density`: `compact`, `normal`, or `comfortable`.
- `details_visible`: boolean controlling the right details panel.
- `visible_columns`: ordered list of column keys.
- `column_widths`: object keyed by column key with integer widths.
- `quick_sector`: string used by the executor shortcut filter.
- `exclude_sca_ses_ste`: boolean for the `SCA/SES/STE` exclusion.
- `column_filters`: object keyed by column key with filter text.

## Rules

- Code defaults are the source of fallback.
- Runtime preferences override defaults only when valid.
- Invalid files must fail clearly at the persistence boundary.
- Column widths are clamped by the presentation model to the supported UI range.
- Visible column preferences must be reconciled against `ColumnCatalog`.
- `system` theme is persisted as a stable value and follows the platform color scheme when Qt
  exposes it.
- Density changes table row height, table header height, and detail panel text sizing only.
- Future migrations require an ADR.
