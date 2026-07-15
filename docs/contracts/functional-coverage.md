# Functional Coverage Contract

This document tracks parity against the Python project at
`/Users/menon/git/SSA_Consulta_Rapida`. The goal is functional coverage, not architectural copying.

## GUI Coverage

| Area                                            | Python reference                                            | C++ target owner                                                                   | Status                             |
| ----------------------------------------------- | ----------------------------------------------------------- | ---------------------------------------------------------------------------------- | ---------------------------------- |
| SQL page load                                   | `gui/ssa/gui_workers.py`, `armazenamento/database.py`       | `ISsaRepository`, `SsaQueryService`                                                | Partial                            |
| General search                                  | `core/app_logic.py::parse_search_terms`, `filter_dataframe` | `SearchParser`, `SqlQueryBuilder`                                                  | Partial                            |
| Column filters                                  | `gui/gui_ssa.py`, `gui/widgets/column_filter_dialog.py`     | `FilterPanelViewModel`, query compiler                                             | Present                            |
| Advanced filters                                | `gui/ssa/gui_filters_advanced_logic.py`                     | `AdvancedFilterSpec`, `SqlQueryBuilder`                                            | Partial                            |
| Exclude SCA/SES/STE                             | `gui/gui_ssa.py`                                            | `SsaPageRequest`                                                                   | Present                            |
| Pagination                                      | `gui/widgets/data_paginator.py`, `interface/cli.py`         | `MainViewModel`, repository paging                                                 | Present                            |
| Sort by header                                  | `gui/gui_ssa.py::on_header_clicked`                         | `MainViewModel`, `SqlQueryBuilder`                                                 | Present                            |
| Visible columns                                 | `gui/widgets/column_manager_dialog.py`                      | `ColumnSettingsModel`, `ColumnSelectorPopup`                                       | Present                            |
| Column widths                                   | `gui/ssa/gui_table.py`                                      | `ColumnSettingsModel`, `SsaTable`                                                  | Present                            |
| Details panel                                   | `gui/ssa/gui_details.py`                                    | `DetailsViewModel`                                                                 | Partial                            |
| Details navigation                              | `gui/ssa/gui_details.py`                                    | `BrowseSelectionCoordinator`, `BrowseOrchestrator`                                 | Present                            |
| Derivadas tree/graph                            | `gui/ssa/gui_details.py`, `armazenamento/derivadas_sync.py` | `DetailsViewModel`, `DerivadasGraphModel`, `SsaWorkflowService`                    | Present                            |
| Export filtered list                            | `exportacao/exporter.py`, `gui/gui_ssa.py`                  | `CsvExportPort`, `SsaWorkflowService`                                              | GUI present, CLI flag present      |
| Import external XLSX                            | `gui/gui_ssa.py::import_external_excel_files`               | `SpreadsheetImportWorkflowPort`, `SsaWorkflowService`                              | Present; XLS isolated              |
| Rescan/update data                              | `gui/ssa/gui_workers.py`, `core/app_logic.py`               | `SpreadsheetImportWorkflowPort`, `SsaWorkflowService`                              | Present                            |
| Consolidate imported input files                | `gui/gui_ssa.py`                                            | `ImportFileStager`, `SpreadsheetImportWorkflowPort`                                | Present                            |
| Clean orphan derivadas                          | `gui/gui_ssa.py::update_derivadas_from_sources`             | `IDerivadasPort`, `SsaWorkflowService`                                             | Present                            |
| Import derivadas sources                        | `armazenamento/derivadas_sync.py`                           | `DerivadasSourceReader`, `SqliteDerivadasPort`, `SsaWorkflowService`                 | Present                            |
| Load other DB                                   | `gui/gui_ssa.py::load_other_database`                       | `SqliteDatabaseValidator`, `DatabaseSwitchViewModel`, `DesktopApplicationLauncher` | Present                            |
| Filter condition history                        | `interface/cli.py::voltar_filtro`                           | `BrowseOrchestrator`, `BrowseViewModel`                                            | GUI present, CLI REPL out of scope |
| Help and About                                  | `gui/gui_ssa.py`                                            | `HelpDialog`, `AboutDialog`, application metadata                                  | Present                            |
| SAM REST fetch                                  | Python `scrap_report` integration                           | `ISamRefreshPort`, `ScrapReportSamRefreshPort`                                     | Present, disabled by default       |
| SAM XLSX to SQLite                              | Python `scrap_report` integration                           | `SamSpreadsheetAdapter`, `SpreadsheetImportWorkflowPort`                           | Present, disabled by default       |
| Vacuum/analyze DB                               | `gui/gui_ssa.py::run_vacuum_analyze`                        | `SqliteMaintenancePort`, `SsaWorkflowService`                                      | CLI present                        |
| Open input/processed/redundant folders and docs | `gui/gui_ssa.py` menu handlers                              | command port variants                                                              | Present                            |
| Preferences/theme/density                       | `gui/ssa/gui_theme.py`, config JSON                         | `IUserPreferencesStore`                                                            | Partial                            |
| Context menus                                   | `gui/gui_ssa.py::show_context_menu`                         | QML menu + view model commands                                                     | Partial                            |
| Header context menu                             | `gui/gui_ssa.py::show_header_context_menu`                  | QML menu + column VM                                                               | Partial                            |

## CLI Coverage

The C++ CLI is flag-based, not an interactive REPL. Python interactive-only
commands (`v`, `m`, `r`, `clear`, `clearall`, `x`, `l/listar/filtros`,
`status-cli`) are out of scope for the flag-based paradigm and are covered
by equivalent flags where noted.

| Area                        | Python reference                             | C++ target owner                                      | Status                             |
| --------------------------- | -------------------------------------------- | ----------------------------------------------------- | ---------------------------------- |
| `--version`                 | `interface/cli_args.py`                      | CLI entrypoint                                        | Present                            |
| `--gui`                     | `interface/cli_args.py`, `main.py`           | desktop entrypoint                                    | Partial                            |
| `--streamlit/--web`         | `main.py`                                    | Not planned for C++ desktop repo                      | Out of scope                       |
| `--force-rescan/--rescan`   | `main.py`, `core/app_logic.py`               | `SsaWorkflowService`                                  | CLI present, adapter contract only |
| `--skip-import`             | `interface/cli_args.py`                      | compatibility flag, no-op by default                  | Present                            |
| `--optimized/--standard`    | `main.py`                                    | import strategy selection                             | CLI present, adapter contract only |
| `--reset-db`                | `main.py`                                    | `SsaWorkflowService`                                  | CLI present                        |
| `--clean-data`              | `main.py`                                    | `SsaWorkflowService`                                  | CLI present, SQLite cleanup only   |
| `--log-level`               | `main.py`                                    | `QLoggingCategory` filter rules in `SsaCliController` | Present                            |
| `--acao backfill`           | `scripts/migracao/backfill_reprocessar.py`   | `SsaCliWorkflowRunner`, `--acao` flag                 | Present                            |
| Search page command         | `interface/cli.py`                           | `SsaBrowseService`, `--search`/`--page`/`--page-size` | Present                            |
| `d #` details               | `interface/cli.py`                           | `SsaBrowseService`, `--details` flag                  | Present                            |
| `ord/ordi/ordn/ordni`       | `interface/cli.py`                           | `--sort`/`--asc`/`--desc` flags                       | Present                            |
| `cols`                      | `interface/cli.py`                           | `--columns`/`--cols` flag                             | Present                            |
| interactive `e name` export | `interface/cli.py`, `exportacao/exporter.py` | `--export` flag                                       | Present                            |
| `status-cli`, debug toggles | `interface/cli.py`                           | `--log-level trace\|debug`                            | Present                            |
| `v` undo filter             | `interface/cli.py`                           | interactive REPL command                              | Out of scope (flag-based CLI)      |
| `m`, `m z` pagination       | `interface/cli.py`                           | interactive REPL pager                                | Out of scope (flag-based CLI)      |
| `r`, `clear`, `clearall`    | `interface/cli.py`                           | interactive REPL filter state                         | Out of scope (flag-based CLI)      |
| `x <term>`                  | `interface/cli.py`                           | interactive REPL filter exclusion                     | Out of scope (flag-based CLI)      |
| `l/listar/filtros`          | `interface/cli.py`                           | interactive REPL filter listing                       | Out of scope (flag-based CLI)      |

## Acceptance Rule

A feature is only marked `Present` when it has:

- a documented contract;
- a non-QML implementation path;
- focused test coverage;
- GUI and/or CLI adapter coverage when applicable;
- no direct SQL in QML or presentation;
- no Qt dependency in `domain`, `query`, or `ports`.

## Notas da GUI 0.9.2

- O historico guarda somente as condicoes aplicadas de busca, filtros por
  coluna, filtros avancados e exclusoes. Resultados e paginas nao sao
  armazenados.
- Ate 10 estados anteriores podem ser restaurados. Aplicar um estado identico
  nao cria uma entrada. Cada undo reinicia a pagina, salva as preferencias e
  executa uma unica consulta com protecao latest-wins.
- O menu de filtros permite voltar varios niveis e copiar a lista textual para
  a area de transferencia.
- Ajuda descreve o contrato C++ real de busca e filtros. Sobre usa a versao da
  aplicacao derivada de `PROJECT_VERSION`.
- A troca de banco valida de forma assincrona e somente leitura um arquivo
  SQLite compativel antes de iniciar uma nova instancia com `--db`. A instancia
  atual so encerra depois que a substituta inicia.
- A consolidacao ocorre depois do commit SQLite. Somente fontes comprovadas
  pelo ultimo import sao movidas para `processadas/` ou
  `processadas/nosurvivor/`, sem sobrescrever destinos existentes.
- A atualizacao SAM REST usa o projeto `scrap_report` local, fica desabilitada
  por padrao e nao armazena senha, token ou segredo.

## Notas da GUI 0.9.7

- O lote SAM e commitado somente depois que todos os setores passam manifesto,
  schema e contagem fisica; exatamente 200 linhas e rejeitado como possivel
  truncamento.
- Derivadas possui importacao explicita para CSV, TXT, TSV, XLSX e XLSM. XLS
  legado exige selecao explicita e preflight visivel do LibreOffice.
- O seletor de colunas abre ancorado ao item real do menu, permanece dentro do
  overlay e recalcula a geometria enquanto a janela e redimensionada.
- A ordem prioritaria de setores e responsaveis e unica entre display e SQL.

## 0.9.0 GUI Notes

- `qtd_derivadas` remains an internal virtual column key/SQL alias, not an
  imported spreadsheet schema column.
- The table label for that key is `Qtd. Derivadas`, and cells with values
  greater than zero open the dedicated details/graph window.
- Relation navigation distinguishes `Origem`, `Derivada`, and `Relacionada`;
  the current SSA is highlighted by position/status without a visible `Atual`
  label.
- The dedicated details window can be opened in multiple independent
  instances, has breadcrumb navigation, copies Mermaid text through `Copiar`,
  and keeps PNG export.
- Advanced text filters use compact cards, preload queued distinct values, keep
  the value popup wider than the inline selector, and make include/exclude token
  semantics visible in the popup.
- Table cells expose copy actions through the context menu, and details fields
  are prioritized and selectable so displayed text can be copied without
  opening SAM.
