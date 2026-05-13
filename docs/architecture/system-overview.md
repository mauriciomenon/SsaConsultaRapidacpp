# System Overview

## Current Truth

This repository is a new C++20 + Qt 6/QML implementation of SSA Consulta Rapida.
It must cover the Python GUI and CLI functional surface without porting the Python architecture.

## Boundaries

- `domain`: records, columns, paging, filter expressions, and value types. No Qt.
- `query`: search parser, SQL query compiler, and query service. No Qt.
- `application`: GUI/CLI use cases that orchestrate domain/query behavior. No Qt.
- `ports`: interfaces used by use cases and external adapters. No Qt.
- `infra`: SQLite and preferences persistence.
- `platform`: desktop/OS integration.
- `presentation`: Qt view models and table models.
- `app/desktop/qml`: visual composition only.

## Main Flow

1. QML calls `MainViewModel`.
2. `MainViewModel` builds `SsaPageRequest`.
3. `PageQueryCoordinator` runs the request outside the UI thread.
4. `SsaBrowseService` owns the browse use case shared by GUI and CLI.
5. `SsaQueryService` calls `ISsaRepository`.
6. `SqliteSsaRepository` uses `SqlQueryBuilder` and returns domain records.
7. `SsaTableModel` exposes the current page, column labels, and widths to QML.

## Presentation Ownership

- `MainViewModel` coordinates page requests, cancellation generation, preferences, and child view
  models, including visual density and detail panel sizing state for QML.
- `PageQueryCoordinator` owns asynchronous page execution for the GUI.
- `UserPreferencesCoordinator` owns debounced preference saves for the GUI.
- `ColumnSettingsModel` owns the editable presentation state for visible columns and widths.
- `SsaTableModel` owns only the current page and column metadata exposed to QML.
- Layout preferences such as detail panel visibility and width stay in presentation state and are
  persisted through `IUserPreferencesStore`.
- QML components bind to view models and do not parse search text, access SQLite, or normalize
  business terms.

## Functional Parity Direction

- Functional coverage is tracked in `docs/contracts/functional-coverage.md`.
- External command boundaries are tracked in `docs/contracts/external-commands.md`.
- GUI and CLI adapters must share domain/query/use-case behavior instead of duplicating rules.
- Import/rescan, derivadas, export, database maintenance, and CLI are planned features, but they
  must enter through ports/use cases instead of direct QML or presentation calls.
- CLI commands must use `application` use cases, never `presentation` models.
- Python mixins, headless PyQt fallbacks, and DataFrame-centric GUI filtering are not architectural
  inputs for this repo.
