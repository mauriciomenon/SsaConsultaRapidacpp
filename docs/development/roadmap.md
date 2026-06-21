# Roadmap: SSA Consulta Rapida C++

Plan derived from `docs/contracts/functional-coverage.md` parity gaps and from
the PyQt6 reference repository (sibling project). Goals: keep C++
memory advantage (already ~7x lower than PyQt6 GUI), close functional parity
without porting Python architecture, and stabilize CI/flaky tests.

## Short term (next 2-3 PRs, this branch base)

### Stabilization
- Fix flaky `OpenPathPolicyTests` (tests 46-48) under parallel ctest. Likely a
  shared temp dir or filesystem race in `platform/OpenPathPolicy.cpp`. Repro:
  `ctest --preset dev -j8` fails, isolated `-R "open path policy"` passes.
- Extend Linux CI job to run `qmllint` (currently only macOS runs it). Linux
  uses `install-qt-action` which provides `qmllint` at
  `$Qt6_DIR/../../../Tools/Qt-6.8.3/gcc_64/bin/qmllint`.
- Add Windows CI job (MSVC) mirroring macOS steps; `msvc.yml` today only does
  code analysis, no build/test.

### Close "Partial" GUI items
- `SQL page load`: verify count+page atomicity under concurrent filters; add
  integration test with real `data/ssas.db` slice.
- `General search`: audit `SearchParser` against
  `core/app_logic.py::parse_search_terms` for token edge cases (quoted strings,
  negation, sector prefixes).
- `Advanced filters`: complete `AdvancedFilterSpec` parity with
  `gui/ssa/gui_filters_advanced_logic.py` (activity, grid, specs modules).
- `Sort by header`: add multi-column sort indicator in QML header (PyQt6 has
  `on_header_clicked` with cycle asc/desc/none).
- `Visible columns` / `Column widths`: persist reorder via
  `ColumnSettingsModel`; PyQt6 `column_manager_dialog.py` supports drag-reorder.
- `Details panel`: port `details_dialog_navigation.py` (next/prev SSA in
  filtered list) and `details_normalization.py` (field display rules).
- `Preferences/theme/density`: add theme dialog parity with
  `gui/ssa/gui_theme_dialog.py` (live preview).

### Close "Partial" / "Missing" CLI items
- `--log-level`: wire to a logging facade in `application/` (no Qt).
- `--acao backfill`: port `scripts/migracao/backfill_reprocessar.py` logic to a
  `application::BackfillAcaoUseCase` with CLI flag.
- `ord/ordi/ordn/ordni`: complete sort request builder parity with
  `interface/cli.py` sort commands.

### CI hygiene
- Pin `install-qt-action` version; consider caching `build/dev` across runs.
- Add `clang-tidy` summary to PR comments (currently fails silently in logs).
- Run `ssa_mem_stress` as a CI smoke with a small fixture DB and assert RSS
  delta < 2 MB over 50 pages (regression guard for memory).

## Long term (multiple PRs, no fixed order)

### Missing GUI features (parity with PyQt6)
- `Details navigation`: next/prev SSA in current filtered list. Requires a
  `DetailsNavigationUseCase` that owns the current row list without reloading.
  Python ref: `gui/ssa/details_dialog_navigation.py`.
- `Derivadas tree/graph`: full sync + graph view. Python ref:
  `gui/ssa/derivadas_sync_controller.py`, `gui/ssa/details_graph_renderer.py`,
  `armazenamento/derivadas_sync.py`. Tracked in `RECOVERY_BACKLOG.md`.
- `Load other DB`: repository factory + command. Python ref:
  `gui/gui_ssa.py::load_other_database`.
- `Context menus`: row and header context menus. Python ref:
  `gui/gui_ssa.py::show_context_menu`, `show_header_context_menu`.
- `Header context menu`: column show/hide + sort reset.

### Missing CLI features (interactive)
- `v` undo filter: CLI filter stack with history.
- `m`, `m z` pagination: interactive pager.
- `r`, `clear`, `clearall`: CLI filter state management.
- `e name` export: interactive export filename prompt.
- `cols`: column catalog adapter.
- `x <term>`: CLI filter state exclusion.
- `l/listar/filtros`: list active filters.
- `status-cli`, debug toggles: diagnostics adapter.

### "Contract only" -> "Present"
These have `I*Port` interfaces but no real adapter. They are the highest-value
parity gaps because the GUI already calls them and gets "not configured" today.
- `Import external XLS/XLSX`: implement `SpreadsheetImportWorkflowPort` end to
  end. Python ref: `gui/gui_ssa.py::import_external_excel_files`,
  `core/import_single_file.py`, `core/import_staging.py`,
  `core/import_postprocess.py`, `core/import_consolidation.py`,
  `core/import_database_rotation.py`. Test with fixture xlsx.
- `Rescan/update data`: implement `IImportWorkflowPort::rescan`. Python ref:
  `gui/ssa/gui_rescan_lifecycle.py`, `core/app_logic.py`.
- `Update derivadas`: implement `IDerivadasPort::syncDerivadas` with real graph
  sync. Python ref: `gui/ssa/derivadas_sync_job.py`,
  `armazenamento/derivadas_sync.py`.

### Architectural improvements
- Split `gui_ssa.py` (247 KB, single class) parity is already avoided; keep
  enforcing via `AGENTS.md` "no god class" rule and code review.
- Add property-based tests for `SearchParser` and `SqlQueryBuilder` (Catch2
  generators or rapidcheck) to match Python `validate_filter_optimizations.py`.
- Add a `docs/contracts/performance-budget.md` documenting measured baselines
  (GUI footprint 35 MB, CLI 23 MB, stress 5 MB) so regressions are detectable.
- Consider `QtQuick.Controls` -> `QtQuick` native items where Controls adds
  overhead without value (e.g. `ScrollBar` already required Controls; audit
  other usages).

### Out of scope (per `docs/contracts/functional-coverage.md`)
- `--streamlit/--web`: not planned for C++ desktop repo.
- Python `CacheManager` (5 buckets): C++ ADR 0002 explicitly avoids
  DataFrame-wide cache; do not port.

## Measurement anchors

| Scenario | Baseline (2026-06-21) | Target |
|---|---:|---:|
| GUI physical footprint (offscreen) | 35.0 MB | < 35 MB |
| GUI RSS (ps -o rss, inflated by Qt COW) | 82 MB | < 82 MB |
| CLI RSS | 23 MB | < 23 MB |
| Stress 200 pages, data path | 5.1 MB | < 6 MB |
| ctest parallel pass rate | flaky (46-48) | 100% |

Re-measure after each merge with `tools/ssa_mem_stress` and `vmmap --summary`.
