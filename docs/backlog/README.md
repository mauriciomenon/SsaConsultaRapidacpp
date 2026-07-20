# Backlog

This directory stores deferred work only. Functional parity is tracked in
`docs/contracts/functional-coverage.md`.

O backlog operacional canonico e `RECOVERY_BACKLOG.md` na raiz. Para a
v0.9.15, use somente o bloco superior `Fonte canonica pos-v0.9.15`; secoes
inferiores sao historico. O handoff ativo e
`docs/plans/2026-07-20-v0.9.15-glm-5.2-handoff.md`.

Categorias atuais:

- Produto: derivadas, filtros ocultos, paginacao SAM e negacao/hifen.
- Evidencia externa: Windows/UNC/SMB, packaging multiplataforma, profiling e
  CI GitLab bloqueado por quota.
- GUI: troca acidental de tema, AA restante e validacao HiDPI.
- Medido/deferido: FTS5, `roleNames()` e LIMIT/OFFSET tipado.

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
