# Functional Coverage Contract

This document tracks parity against the Python project at
`/Users/menon/git/SSA_Consulta_Rapida`. The goal is functional coverage, not architectural copying.

## GUI Coverage

| Area | Python reference | C++ target owner | Status |
|---|---|---|---|
| SQL page load | `gui/ssa/gui_workers.py`, `armazenamento/database.py` | `ISsaRepository`, `SsaQueryService` | Partial |
| General search | `core/app_logic.py::parse_search_terms`, `filter_dataframe` | `SearchParser`, `SqlQueryBuilder` | Partial |
| Column filters | `gui/gui_ssa.py`, `gui/widgets/column_filter_dialog.py` | `FilterPanelViewModel`, query compiler | Present |
| Advanced filters | `gui/ssa/gui_filters_advanced_logic.py` | `AdvancedFilterSpec`, `SqlQueryBuilder` | Partial |
| Exclude SCA/SES/STE | `gui/gui_ssa.py` | `SsaPageRequest` | Present |
| Pagination | `gui/widgets/data_paginator.py`, `interface/cli.py` | `MainViewModel`, repository paging | Present |
| Sort by header | `gui/gui_ssa.py::on_header_clicked` | `MainViewModel`, `SqlQueryBuilder` | Present |
| Visible columns | `gui/widgets/column_manager_dialog.py` | `ColumnSettingsModel`, `ColumnSelectorPopup` | Present |
| Column widths | `gui/ssa/gui_table.py` | `ColumnSettingsModel`, `SsaTable` | Present |
| Details panel | `gui/ssa/gui_details.py` | `DetailsViewModel` | Partial |
| Details navigation | `gui/ssa/gui_details.py` | `BrowseSelectionCoordinator`, `BrowseOrchestrator` | Present |
| Derivadas tree/graph | `gui/ssa/gui_details.py`, `armazenamento/derivadas_sync.py` | `IImportWorkflowPort`, `SsaWorkflowService` | GUI present, graph view missing |
| Export filtered list | `exportacao/exporter.py`, `gui/gui_ssa.py` | `CsvExportPort`, `SsaWorkflowService` | GUI present, CLI flag present |
| Import external XLS/XLSX | `gui/gui_ssa.py::import_external_excel_files` | `SpreadsheetImportWorkflowPort`, `SsaWorkflowService` | Present |
| Rescan/update data | `gui/ssa/gui_workers.py`, `core/app_logic.py` | `SpreadsheetImportWorkflowPort`, `SsaWorkflowService` | Present |
| Update derivadas | `gui/gui_ssa.py::update_derivadas_from_sources` | `IDerivadasPort::syncDerivadas`, `SsaWorkflowService` | GUI present, graph view missing |
| Load other DB | `gui/gui_ssa.py::load_other_database` | repository factory + command | Missing |
| Vacuum/analyze DB | `gui/gui_ssa.py::run_vacuum_analyze` | `SqliteMaintenancePort`, `SsaWorkflowService` | CLI present |
| Open input/processed/redundant folders and docs | `gui/gui_ssa.py` menu handlers | command port variants | Present |
| Preferences/theme/density | `gui/ssa/gui_theme.py`, config JSON | `IUserPreferencesStore` | Partial |
| Context menus | `gui/gui_ssa.py::show_context_menu` | QML menu + view model commands | Partial |
| Header context menu | `gui/gui_ssa.py::show_header_context_menu` | QML menu + column VM | Partial |

## CLI Coverage

The C++ CLI is flag-based, not an interactive REPL. Python interactive-only
commands (`v`, `m`, `r`, `clear`, `clearall`, `x`, `l/listar/filtros`,
`status-cli`) are out of scope for the flag-based paradigm and are covered
by equivalent flags where noted.

| Area | Python reference | C++ target owner | Status |
|---|---|---|---|
| `--version` | `interface/cli_args.py` | CLI entrypoint | Present |
| `--gui` | `interface/cli_args.py`, `main.py` | desktop entrypoint | Partial |
| `--streamlit/--web` | `main.py` | Not planned for C++ desktop repo | Out of scope |
| `--force-rescan/--rescan` | `main.py`, `core/app_logic.py` | `SsaWorkflowService` | CLI present, adapter contract only |
| `--skip-import` | `interface/cli_args.py` | compatibility flag, no-op by default | Present |
| `--optimized/--standard` | `main.py` | import strategy selection | CLI present, adapter contract only |
| `--reset-db` | `main.py` | `SsaWorkflowService` | CLI present |
| `--clean-data` | `main.py` | `SsaWorkflowService` | CLI present, SQLite cleanup only |
| `--log-level` | `main.py` | `QLoggingCategory` filter rules in `SsaCliController` | Present |
| `--acao backfill` | `scripts/migracao/backfill_reprocessar.py` | `SsaCliWorkflowRunner`, `--acao` flag | Present |
| Search page command | `interface/cli.py` | `SsaBrowseService`, `--search`/`--page`/`--page-size` | Present |
| `d #` details | `interface/cli.py` | `SsaBrowseService`, `--details` flag | Present |
| `ord/ordi/ordn/ordni` | `interface/cli.py` | `--sort`/`--asc`/`--desc` flags | Present |
| `cols` | `interface/cli.py` | `--columns`/`--cols` flag | Present |
| interactive `e name` export | `interface/cli.py`, `exportacao/exporter.py` | `--export` flag | Present |
| `status-cli`, debug toggles | `interface/cli.py` | `--log-level trace\|debug` | Present |
| `v` undo filter | `interface/cli.py` | interactive REPL command | Out of scope (flag-based CLI) |
| `m`, `m z` pagination | `interface/cli.py` | interactive REPL pager | Out of scope (flag-based CLI) |
| `r`, `clear`, `clearall` | `interface/cli.py` | interactive REPL filter state | Out of scope (flag-based CLI) |
| `x <term>` | `interface/cli.py` | interactive REPL filter exclusion | Out of scope (flag-based CLI) |
| `l/listar/filtros` | `interface/cli.py` | interactive REPL filter listing | Out of scope (flag-based CLI) |

## Acceptance Rule

A feature is only marked `Present` when it has:

- a documented contract;
- a non-QML implementation path;
- focused test coverage;
- GUI and/or CLI adapter coverage when applicable;
- no direct SQL in QML or presentation;
- no Qt dependency in `domain`, `query`, or `ports`.
