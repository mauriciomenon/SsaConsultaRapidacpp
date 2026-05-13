# GUI Behavior Contract

## Main Surface

- Toolbar with SAM, refresh, and cancel actions.
- Menus or command surfaces for import, rescan, derivadas, export, database maintenance, settings,
  help, and folder opening must call view model commands backed by ports.
- General search row with apply, clear, page navigation, and page size.
- Filter panel with `SCA/SES/STE` exclusion, quick executor, column filter, week/year
  filters, derivation mode, and reprogrammed-only filter.
- Table with the current SQL page only.
- Table header labels and widths come from `ColumnCatalog`.
- Header click sorts by that column and toggles ascending/descending on repeated click.
- Sorting by column `numero_ssa` applies secondary status ordering after primary SSA sort by design.
- Details panel for the selected SSA.
- Preferences dialog with theme selection, visual density, visible column selection, column width
  editing, select all, defaults reset, detail panel visibility, detail panel width, local column
  filtering, apply, and close actions.
- Status bar with loading, error, and row/page information.

## Preserved Behavior

- General search keeps the Python semantics documented in `query-contract.md`.
- Closed statuses `SCA`, `SES`, and `STE` can be excluded.
- Resetting filters restores the default `SCA/SES/STE` exclusion.
- Advanced filters are query parameters, not QML logic:
  week/year use one of `semana_cadastro`, `semana_programada`, or `semana_executada`;
  derivation mode is `all`, `root`, or `derived`; reprogrammed-only checks reprogramming
  counters.
- Details are read from the selected row and can open the selected SSA externally.
- External actions return explicit command results: success, not implemented, rejected, or failed.
- The GUI reports errors visibly instead of silently self-healing.
- Queries run outside the GUI thread. Cancel invalidates the active request and prevents stale
  results from replacing the current screen state.
- Column visibility, column widths, page size, filters, advanced filters, detail panel visibility,
  detail panel width, density, and theme are persisted through `IUserPreferencesStore`.
- The table must keep the main screen useful on a desktop viewport: toolbar and search stay
  visible, the table owns the central area, and details stay on the right.
- Future tabs, graph views, and alternate result views must reuse the same query/use-case contracts.

## Visual Customization

- Theme values are `system`, `light`, `dark`, and `gruvbox`. `system` follows the platform color
  scheme when Qt exposes it.
- Density values are `compact`, `normal`, and `comfortable`; density changes presentation sizing,
  not query behavior.
- Visible columns are ordered by `ColumnCatalog`; preferences only choose inclusion and width.
- The GUI must not use column customization to introduce query aliases or business synonyms.
- At least one visible column must remain selected.
- Width edits are stored by column key, not by visual index.
- Closing the preferences dialog without applying discards pending column visibility and width
  edits.
- Detail panel visibility is applied immediately because it is a screen layout preference, not a
  query preference.
- Detail panel width is applied immediately and does not trigger a query reload.

## Not Preserved

- Python mixins.
- Global DataFrame filtering.
- Retired worker global lists.
- Hidden fallback behavior.
