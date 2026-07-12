# Preferences Schema

## Version 12

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
- `quick_sector`: string used by the executor shortcut filter. The default startup value is
  `IEE3`. Empty means no quick sector filter only when the user or a saved manual state explicitly
  clears it. Distinct value lists still prioritize known sector families in display order without
  hiding other values.
- `exclude_sca_ses_ste`: boolean for the `SCA/SES/STE` exclusion.
- `column_filters`: object keyed by column key with filter text.
- `advanced_text_filters`: object keyed by column key with advanced filter text.
- `advanced_week_column_key`: week column key used by advanced week/year filters.
- `advanced_year`: string year filter, empty when disabled.
- `advanced_week`: string week filter, empty when disabled.
- `issue_year`: string issue year filter over `semana_cadastro`, empty when disabled.
- `execution_year`: string execution year filter over `semana_executada`, empty when disabled.
- `reprogramming_mode`: `eq`, `lte`, or `gte` comparison for selected values.
- `reprogramming_values`: comma-separated reprogramming counts, empty when disabled.
- `issue_week_start`: string lower bound for issue `AnoSemana`, empty when disabled.
- `issue_week_end`: string upper bound for issue `AnoSemana`, empty when disabled.
- `execution_week_start`: string lower bound for execution `AnoSemana`, empty when disabled.
- `execution_week_end`: string upper bound for execution `AnoSemana`, empty when disabled.
- `derivation_mode`: `all`, `root`, or `derived`.
- `only_reprogrammed`: boolean for reprogrammed-only filter.
- `saved_filters`: ordered array of named filter snapshots.

## Rules

- Code defaults are the source of fallback.
- Runtime preferences override defaults only when valid.
- Invalid files must fail clearly at the persistence boundary.
- Missing, non-integer, non-positive, and future schema versions are invalid.
- Preference and filter preset JSON files are limited to 1 MiB.
- At most 200 saved filters are accepted. Names are limited to 128 characters and filter
  expressions are limited to 4096 characters.
- Column widths are clamped by the presentation model to the supported UI range.
- Visible column preferences must be reconciled against `ColumnCatalog`.
- Duplicate visible column keys are invalid.
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
- `reprogramming_equals` is accepted only while reading legacy schema 12 documents. When
  `reprogramming_values` is empty, its value is migrated to that field. Values take priority over
  the legacy field and `only_reprogrammed`; new documents never write `reprogramming_equals`.
- The current default visible table order is:
  `numero_ssa`, `situacao`, `localizacao_codigo`, `setor_emissor`, `setor_executor`,
  `qtd_derivadas`, `descricao_ssa`, `semana_cadastro`, `solicitante`, `derivada_de`,
  `responsavel_programacao`, `responsavel_execucao`, `semana_programada`, `semana_executada`,
  `descricao_execucao`.
- `data_cadastro` remains a valid catalog column, but it is no longer visible by default.
- `derivada_de` is displayed as `Der. de`, uses tooltip `Derivada da SSA:`, and defaults to
  width 88 so an ordinary SSA number is not elided.

## Migrations

- Version 3 to 4: restores the default `quick_sector=IEE3` when a saved empty quick sector is the
  only active filter. Manual states with other filters keep their quick sector state.
- Version 10 to 11: migrates only exact old default visible column lists to the current default
  order, removes `data_cadastro` from default visibility, moves `derivada_de` after
  `solicitante`, and raises legacy default `derivada_de` width from 62 to 88. Manual column order
  and non-default widths remain user-owned.
- Version 11 to 12: removes `data_cadastro` from the legacy default visibility and places
  `semana_cadastro`, `solicitante`, and `derivada_de` in the current default order.
