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

### Entregas da 0.9.2

- Historico de ate 10 estados de filtros na GUI, retorno de varios niveis e
  copia textual para a area de transferencia.
- Ajuda baseada no contrato C++ e Sobre com versao de `PROJECT_VERSION`.
- Troca assincrona para outro banco SQLite validado em modo somente leitura,
  iniciando a nova instancia antes de encerrar a atual.
- Consolidacao pos-commit das fontes efetivamente importadas em
  `processadas/` e `processadas/nosurvivor/`, com nome unico e rename sem
  sobrescrita.
- Atualizacao SAM REST via `scrap_report`, com preflight explicito, importacao
  all-or-none, timer persistido, single-flight por instancia e cancelamento no
  shutdown.

### Entregas da 0.9.3

- Cancelamento terminal real em consultas, filtros distinct, exportacao,
  workflows, validacao de banco, preferencias e presets.
- Latest-wins sem publicacao stale, com retry preservado quando a aplicacao de
  filtros falha.
- Atomicidade SQLite antes/depois do commit, rollback verificado e testes de
  morte por subprocesso com `integrity_check`.
- Supervisor multiplataforma de arvores de processos, staging atomico e
  limpeza de temporarios em copia, XLS e XLSX.
- Shutdown responsivo com status contextual, `Cancelando...`, barreira de
  terminais e confirmacao forcada sem espera na thread GUI.

### Entregas da 0.9.4

- Mensagem segura e diagnostico tecnico separados quando o conversor XLS nao
  esta disponivel.
- Gate Linux estavel para retry de limpeza SAM: simulacao por permissao roda
  em POSIX nao-root e e explicitamente pulada quando o runner e root.
- Fluxos macOS interativo, offscreen, incremental e release documentados com
  os scripts versionados usados em cada caso.

### Entregas da 0.9.5

- Full rescan rejeita planilha sem cabecalho reconhecido ou sem linhas validas
  antes do commit, preservando banco e fontes anteriores.
- Staging distingue artefatos owned, cancelamento, falha primaria e falha real
  de cleanup sem deixar temporarios invisiveis.
- Diagnosticos de importacao separam razao publica segura de detalhe tecnico,
  usam contagens consistentes para XLS e contagens de preflight para
  consolidacao.
- Teste de shutdown prova que o supervisor volta a aceitar processos somente
  depois de uma barreira bem-sucedida.
- Configuracao Markdown do projeto elimina MD024 falso em changelog sem
  desabilitar duplicatas reais no mesmo nivel.

## Long term (multiple PRs, no fixed order)

### Missing GUI features (parity with PyQt6)

- `Details navigation`: next/prev SSA in current filtered list exists in the
  main details panel. The dedicated details window has independent instances,
  breadcrumb navigation, clickable graph nodes, and relation badges.
- `Derivadas tree/graph`: graph view exists with clickable nodes, explicit
  Mermaid copy, PNG export, node statuses, and table access from
  `Qtd. Derivadas`. Remaining gap: deeper multi-level derivada traversal
  beyond the current direct-relation repository contract.
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

### SAM depois da 0.9.2

- Avaliar XPath/Playwright somente se o contrato REST deixar de atender o
  fluxo operacional.
- Definir contrato separado para credenciais com Keychain, Credential Manager
  e Secret Service antes de aceitar escopos com usuario, senha ou token.
- Avaliar paginacao do `scrap_report`. A 0.9.2 consulta no maximo 200 registros
  por setor em cada rodada.
- Se varias instancias da aplicacao forem executadas, cada uma possui seu
  proprio single-flight. Coordenacao entre processos fica fora deste release.

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
- Avaliar cache estavel de `roleNames()` somente em ciclo de performance com
  medicao de alocacao e sem alterar o contrato dos modelos Qt.

### Out of scope (per `docs/contracts/functional-coverage.md`)

- `--streamlit/--web`: not planned for C++ desktop repo.
- Python `CacheManager` (5 buckets): C++ ADR 0002 explicitly avoids
  DataFrame-wide cache; do not port.

## Measurement anchors

| Scenario                                | Baseline (2026-06-21) |  Target |
| --------------------------------------- | --------------------: | ------: |
| GUI physical footprint (offscreen)      |               35.0 MB | < 35 MB |
| GUI RSS (ps -o rss, inflated by Qt COW) |                 82 MB | < 82 MB |
| CLI RSS                                 |                 23 MB | < 23 MB |
| Stress 200 pages, data path             |                5.1 MB |  < 6 MB |
| ctest parallel pass rate                |         flaky (46-48) |    100% |

Re-measure after each merge with `ssa_mem_stress` (CMake target under
`SSA_BUILD_TESTS`) and `vmmap --summary`.
