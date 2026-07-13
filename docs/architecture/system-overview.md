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

## CMake Targets

The CMake graph is the source of truth for build ownership:

- `ssa_core`: `src/domain` and `src/query`.
- `ssa_application`: use cases in `src/application`, linked to `ssa_core`.
- `ssa_infra`: SQLite, import/export, and JSON preferences, linked to `ssa_core`, SQLite,
  Qt Core, and `miniz`.
- `ssa_platform`: desktop path, URL, and external command adapters, linked to `ssa_core`,
  Qt Core, and Qt Gui.
- `ssa_presentation_tables`: table/detail formatting and Qt table models.
- `ssa_presentation_models`: Qt view models, filter state, coordinators, and async browse flow.
- `ssa_presentation`: interface aggregate for presentation targets.
- `ssa_consulta_rapida`: Qt/QML desktop executable.
- `ssa_cli`: CLI controller support library.
- `ssa_consulta_rapida_cli`: CLI executable.
- Test targets: `ssa_unit_tests`, `ssa_integration_tests`, `ssa_qt_presentation_tests`,
  `ssa_qt_presentation_filter_tests`, `ssa_qt_filter_panel_tests`, and non-Windows
  `ssa_mem_stress`.

The GUI executable links presentation, infrastructure, and platform adapters at the composition
root. QML stays below `app/desktop/qml` and must not own SQL, persistence, or filter business
rules.

## Presentation Ownership

- `MainViewModel` coordinates page requests, cancellation generation, preferences, and child view
  models, including visual density and detail panel sizing state for QML.
- `PageQueryCoordinator` owns asynchronous page execution for the GUI.
- `BrowseOrchestrator` owns the volatile bounded history of applied filter
  conditions. It never stores result pages in that history.
- `UserPreferencesCoordinator` owns debounced preference saves for the GUI.
- `DatabaseSwitchViewModel` owns asynchronous validation and replacement
  process startup without changing the active repository in place.
- `WorkflowCommandViewModel` owns SAM refresh settings, the per-instance timer,
  and the visible single-flight state. Process execution remains in platform.
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
- Import/rescan with post-commit consolidation, derivadas, export, database
  maintenance, alternate database startup, SAM REST refresh, and CLI enter
  through ports and use cases instead of direct QML calls.
- CLI commands must use `application` use cases, never `presentation` models.
- Python mixins, headless PyQt fallbacks, and DataFrame-centric GUI filtering are not architectural
  inputs for this repo.

## Runtime Contracts

- Preferences are persisted through `IUserPreferencesStore` and encoded by
  `UserPreferencesJsonCodec`; the current schema is documented in
  `docs/contracts/preferences-schema.md`.
- Column labels, default visibility, and default widths come from `ColumnCatalog`. Runtime
  preferences may override them only after validation against the catalog.
- Advanced text filters and status/executor shortcuts share the presentation filter state before
  reaching query compilation. QML renders that state and calls view-model commands only.
- Derivation data is represented by record fields such as `derivada_de` and `qtd_derivadas`;
  graph ownership is split between domain graph rules and presentation/QML rendering.
- Filter undo is runtime-only and bounded to 10 previously applied conditions.
  Reapplying the current identical state does not add an entry; older
  non-consecutive states may repeat.
- SAM settings persist in the optional `sam_refresh` preference object. The
  adapter executes `uv` without a shell, validates every manifest, and exposes
  no credential field.
