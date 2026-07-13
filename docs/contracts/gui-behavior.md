# GUI Behavior Contract

## Main Surface

- Toolbar with SAM, refresh, and cancel actions.
- Menus or command surfaces for import, rescan, derivadas, export, database maintenance, settings,
  help, and folder opening must call view model commands backed by ports.
- General search row with apply, clear, page navigation, and page size.
- A simple undo action before the search input, plus a Filters menu for undoing
  multiple levels and copying the textual history.
- Filter panel with `SCA/SES/STE` exclusion, quick executor, column filter, week/year
  filters, derivation mode, and reprogrammed-only filter.
- Table with the current SQL page only.
- Table header labels and widths come from `ColumnCatalog`.
- Header width edits are applied through `ColumnSettingsModel`, persisted by column key, and do not
  reload the SQL page.
- Header click sorts by that column and cycles ascending, descending, then no explicit sort.
- Sorting by column `numero_ssa` applies status-last ordering before SSA ordering by design.
- Header right-click exposes column filter focus, hide-current-column, and column selector actions.
- Details panel for the selected SSA.
- Preferences dialog with theme selection, visual density, visible column selection, column width
  editing, select all, defaults reset, detail panel visibility, detail panel width, local column
  filtering, apply, and close actions.
- Status bar with loading, error, and row/page information.
- Help and About dialogs loaded only while open. About displays the runtime
  application version derived from `PROJECT_VERSION`.
- A FileDialog for selecting another SQLite database and a SAM refresh settings
  dialog backed by view-model commands.

## Preserved Behavior

- General search keeps the Python semantics documented in `query-contract.md`.
- Closed statuses `SCA`, `SES`, and `STE` can be excluded.
- Resetting filters restores the default, which shows `SCA`, `SES`, and `STE` (the
  `SCA/SES/STE` exclusion checkbox starts unchecked).
- Advanced filters are query parameters, not QML logic:
  week/year use one of `semana_cadastro`, `semana_programada`, or `semana_executada`;
  derivation mode is `all`, `root`, or `derived`; reprogrammed-only checks reprogramming
  counters.
- Details are read from the selected row and can open the selected SSA externally.
- External actions return explicit command results: success, not implemented, rejected, or failed.
- The GUI reports errors visibly instead of silently self-healing.
- Queries run outside the GUI thread. Cancel invalidates the active request and prevents stale
  results from replacing the current screen state.
- Filter history stores only applied search, column, advanced, and exclusion
  conditions. It keeps at most 10 previously applied states and never stores
  filtered rows. Reapplying the current identical state does not add an entry;
  older non-consecutive states may repeat.
- Undo restores one or more filter states, resets to page 1, requests one
  preference save, and starts one query. Reapplying the restored state remains
  protected by the normal latest-wins query contract.
- Clearing general search starts one query rather than a duplicated clear plus
  apply sequence.
- Column visibility, column widths, page size, filters, advanced filters, detail panel visibility,
  detail panel width, density, and theme are persisted through `IUserPreferencesStore`.
- The table must keep the main screen useful on a desktop viewport: toolbar and search stay
  visible, the table owns the central area, and details stay on the right.
- Future tabs, graph views, and alternate result views must reuse the same query/use-case contracts.

## Banco alternativo

- A GUI accepts only a local file selected by the user.
- Validation runs outside the GUI thread and opens SQLite read-only.
- The file must be regular, pass `PRAGMA quick_check(1)`, contain `ssa_table`,
  provide the compatible storage schema, and contain at least one record.
- A valid selection starts a new application instance with `--db`. The current
  instance exits only after the replacement process reports that it started.
- Validation or launch failure keeps the current session intact and displays
  an objective error.

## Consolidacao de entrada

- A consolidation manifest is assembled only after the SQLite write session
  commits successfully.
- Only source files associated with the committed import are eligible.
- Sources with valid rows move to `processadas/`; sources without valid rows
  move to `processadas/nosurvivor/`.
- Unknown, failed, pending, or uncommitted files stay visible in the input
  directory.
- Existing destinations are never overwritten. A unique name and an atomic
  no-replace rename are used for each move.
- Cancellation or a post-commit move failure remains visible as a warning and
  does not hide the successful database commit.

## Atualizacao SAM REST

- The feature is disabled by default. Manual and automatic refresh require the
  explicit enable switch.
- Settings persist through `IUserPreferencesStore`; passwords, tokens, and
  secrets are not accepted or stored.
- Preflight requires an executable `uv`, a complete local `scrap_report`
  project, a non-empty regular CA file, an HTTPS URL without credentials,
  query, or fragment, unique uppercase executor sectors, and scope `consulta`.
- Each refresh requests the REST `panorama` profile with TLS verification,
  details enabled, four years, and a limit of 200 records per sector.
- Every sector must return a strict successful manifest and one fresh non-empty
  XLSX. A partial batch is rejected and nothing from it is imported.
- A complete batch is imported once through the optimized spreadsheet import
  port. Temporary artifact cleanup is attempted on every terminal path. A
  cleanup failure remains visible and becomes a warning when import already
  committed successfully.
- Only one SAM refresh runs per application instance. A second trigger while
  it is running is a no-op. Shutdown requests cancellation and waits for the
  worker to finish.
- Refresh success reloads the GUI only after the spreadsheet import commits.
  Stale or canceled operations cannot publish a terminal result.

## Visual Customization

- Theme values are accepted by `UserPreferenceDefaults`, including `system`, `ssa-dark`, `light`,
  `dark`, and `gruvbox`. `system` follows the platform color scheme when Qt exposes it.
- Density values are `compact`, `normal`, and `comfortable`; density changes presentation sizing,
  not query behavior.
- Visible columns are ordered by `ColumnCatalog`; preferences only choose inclusion and width.
- The GUI must not use column customization to introduce query aliases or business synonyms.
- At least one visible column must remain selected.
- Width edits are stored by column key, not by visual index.
- Toolbar column selection opens a popup with staged changes; only its explicit apply action
  persists through `IUserPreferencesStore`.
- Header hide actions apply immediately and persist through `IUserPreferencesStore`.
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
