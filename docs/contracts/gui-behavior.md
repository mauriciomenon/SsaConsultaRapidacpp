# GUI Behavior Contract

## Main Surface

- Toolbar with SAM, refresh, and cancel actions.
- General search row with apply, clear, page navigation, and page size.
- Filter panel with closed-status exclusion, quick executor, and column filter.
- Table with the current SQL page only.
- Table header labels and widths come from `ColumnCatalog`.
- Header click sorts by that column and toggles ascending/descending on repeated click.
- Details panel for the selected SSA.
- Preferences dialog with theme selection, visible column selection, column width editing, select
  all, defaults reset, apply, and close actions.
- Status bar with loading, error, and row/page information.

## Preserved Behavior

- General search keeps the Python semantics documented in `query-contract.md`.
- Closed statuses `SCA`, `SES`, and `STE` can be excluded.
- Details are read from the selected row and can open the selected SSA externally.
- The GUI reports errors visibly instead of silently self-healing.
- Queries run outside the GUI thread. Cancel invalidates the active request and prevents stale
  results from replacing the current screen state.
- Column visibility, column widths, page size, filters, and theme are persisted through
  `IUserPreferencesStore`.
- The table must keep the main screen useful on a desktop viewport: toolbar and search stay
  visible, the table owns the central area, and details stay on the right.

## Visual Customization

- Theme values are `system`, `light`, and `dark`. `system` currently uses the light token set until
  platform theme detection is implemented.
- Visible columns are ordered by `ColumnCatalog`; preferences only choose inclusion and width.
- The GUI must not use column customization to introduce query aliases or business synonyms.
- At least one visible column must remain selected.
- Width edits are stored by column key, not by visual index.
- Closing the preferences dialog without applying discards pending column visibility and width
  edits.

## Not Preserved

- Python mixins.
- Global DataFrame filtering.
- Retired worker global lists.
- Hidden fallback behavior.
