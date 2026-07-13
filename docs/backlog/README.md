# Backlog

This directory stores deferred work only. Functional parity is tracked in
`docs/contracts/functional-coverage.md`.

## Deferred

- Complete the remaining GUI and CLI gaps tracked in
  `docs/contracts/functional-coverage.md` without adding an interactive CLI
  unless product scope changes.
- Add database backup through `IDatabaseMaintenancePort` without exposing
  SQLite operations to presentation.
- Add credential-backed SAM scopes only after a cross-platform secret-storage
  contract exists.
- Evaluate SAM pagination beyond the current limit of 200 records per sector.
- Coordinate SAM refresh across application processes only if concurrent
  instances become an operational requirement.
- Performance cache only after measured SQL hotspot.
- Column preferences proxy filter if the column catalog becomes large enough to
  show measurable QML delegate filtering cost.
- Debounced or async preference saves if measured disk latency causes visible
  UI stalls during repeated preference changes.
