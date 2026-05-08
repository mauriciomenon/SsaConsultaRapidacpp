# GUI Behavior Contract

## Main Surface

- Toolbar with SAM, refresh, and cancel actions.
- General search row with apply, clear, page navigation, and page size.
- Filter panel with closed-status exclusion, quick executor, and column filter.
- Table with the current SQL page only.
- Details panel for the selected SSA.
- Status bar with loading, error, and row/page information.

## Preserved Behavior

- General search keeps the Python semantics documented in `query-contract.md`.
- Closed statuses `SCA`, `SES`, and `STE` can be excluded.
- Details are read from the selected row and can open the selected SSA externally.
- The GUI reports errors visibly instead of silently self-healing.

## Not Preserved

- Python mixins.
- Global DataFrame filtering.
- Retired worker global lists.
- Hidden fallback behavior.
