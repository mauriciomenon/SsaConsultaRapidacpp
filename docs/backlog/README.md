# Backlog

This directory stores deferred work only. Functional parity is tracked in
`docs/contracts/functional-coverage.md`.

## Deferred

- Implement import/rescan through explicit ports/use cases.
- Implement derivadas sync and graph view through explicit ports/use cases.
- Implement CLI parity over the same domain/query/use-case contracts used by GUI.
- Implement database maintenance commands without exposing SQLite operations to presentation.
- Windows installer.
- Linux package.
- Performance cache only after measured SQL hotspot.
- Column preferences proxy filter if the column catalog becomes large enough to
  show measurable QML delegate filtering cost.
- Debounced or async preference saves if measured disk latency causes visible
  UI stalls during repeated preference changes.
