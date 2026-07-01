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
- `Visible columns` / `Column widths`: column hide/show and width persistence
  are present. Remaining gap: persist reorder via `ColumnSettingsModel`; PyQt6
  `column_manager_dialog.py` supports drag-reorder.
- `Details panel`: field display rules remain under review. The current C++
  path has dedicated details window, relation graph, Mermaid copy action, PNG
  export, relation badges, breadcrumb navigation, and relation-node navigation.
- `Preferences/theme/density`: add theme dialog parity with
  `gui/ssa/gui_theme_dialog.py` (live preview).

### Close "Partial" / "Missing" CLI items
- CLI flag parity is complete: `--log-level`, `--acao backfill`, `--cols`,
  `--export`, `--sort`/`--asc`/`--desc` are all Present (see
  `docs/contracts/functional-coverage.md`). The remaining Python CLI items
  (`v`, `m`, `r`, `clear`, `clearall`, `x`, `l/listar/filtros`)
   are interactive REPL commands out of scope for the
   flag-based C++ CLI. Note: `status-cli` maps to `--log-level` and is Present.

### CI hygiene
- Pin `install-qt-action` version; consider caching `build/dev` across runs.
- Add `clang-tidy` summary to PR comments (currently fails silently in logs).
- Memory regression smoke: `ssa_mem_stress` runs on Linux CI with fixture DB
  (done locally, needs CI validation).

## Long term (multiple PRs, no fixed order)

### Missing GUI features (parity with PyQt6)
- `Details navigation`: next/prev SSA in current filtered list exists in the
  main details panel. The dedicated details window has independent instances,
  breadcrumb navigation, clickable graph nodes, and relation badges.
- `Derivadas tree/graph`: graph view exists with clickable nodes, explicit
  Mermaid copy, PNG export, node statuses, and table access from
  `Qtd. Derivadas`. Remaining gap: deeper multi-level derivada traversal
  beyond the current direct-relation repository contract.
- `Load other DB`: repository factory + command. Python ref:
  `gui/gui_ssa.py::load_other_database`.
- `Context menus`: row/cell menu exists for copy, SAM open, details window,
  visible columns, and derivation SVG copy. Remaining gap: broader Python
  action parity. Python ref: `gui/gui_ssa.py::show_context_menu`.
- `Header context menu`: header menu exists for filter focus, hide column, and
  column selector. Remaining gap: sort reset action.

### Missing CLI features (interactive REPL, out of scope)
The C++ CLI is flag-based, not an interactive REPL. These Python interactive
commands are out of scope unless a REPL mode is explicitly added:
- `v` undo filter, `m`/`m z` pager, `r`/`clear`/`clearall` filter state,
  `x <term>` exclusion, `l/listar/filtros` listing.

### "Contract only" -> "Present" (completed)
- `Import external XLS/XLSX`: done. `SpreadsheetImportWorkflowPort` wired in
  `DesktopMainViewModelFactory`; QML invokes via Importacao menu + FileDialog.
- `Rescan/update data`: done. `IImportWorkflowPort::rescan` wired; QML invokes
  via Importacao menu, Manutencao menu, and toolbar button.
- `Update derivadas`: `syncDerivadas` C++ impl complete; QML trigger added to
  Importacao and Manutencao menus. Graph view, Mermaid copy, and PNG export are
  present.

### Architectural improvements
- Split `gui_ssa.py` (247 KB, single class) parity is already avoided; keep
  enforcing via `AGENTS.md` "God class" prohibition and code review.
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

Re-measure after each merge with `ssa_mem_stress` (CMake target under
`SSA_BUILD_TESTS`) and `vmmap --summary`.
