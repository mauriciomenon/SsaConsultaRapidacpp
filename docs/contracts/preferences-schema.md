# Preferences Schema

## Version 1

Runtime file: `ssa_cpp_preferences.json`.

Current fields:

- `schema_version`: integer.
- `page_size`: integer from 10 to 500.
- `theme`: `system`, `light`, or `dark`.
- `visible_columns`: ordered list of column keys.
- `column_widths`: object keyed by column key with integer widths.
- `quick_sector`: string used by the executor shortcut filter.
- `exclude_closed_statuses`: boolean for the `SCA/SES/STE` exclusion.
- `column_filters`: object keyed by column key with filter text.

## Rules

- Code defaults are the source of fallback.
- Runtime preferences override defaults only when valid.
- Invalid files must fail clearly at the persistence boundary.
- Future migrations require an ADR.
