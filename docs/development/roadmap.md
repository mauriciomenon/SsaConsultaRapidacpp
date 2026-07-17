# Roadmap: SSA Consulta Rapida C++

Plan derived from `docs/contracts/functional-coverage.md` parity gaps and from
the PyQt6 reference repository (sibling project). Goals: keep C++
memory advantage (already ~7x lower than PyQt6 GUI), close functional parity
without porting Python architecture, and stabilize CI/flaky tests.

## Short term apos v0.9.10

### Stabilization

- Incorporar o feedback de Cursor GLM e OpenCode somente depois de reproduzir
  cada finding no HEAD e classificar como valido, duplicado, obsoleto ou fora
  de escopo.
- Completar pointer real por familia dos menus, preservando os contratos de
  handler ja verdes e sem alterar labels, IDs ou layout.
- Fechar conversao de URL e fonte unica de geometria do grafo com casos
  macOS, Windows/UNC e Linux antes de alterar o QML.
- Separar CPU, idle e latencia do prefetch do custo de startup QtTest e reparar
  `clang-tidy`/`.qmltypes` antes de promover essas ferramentas a gates reais.
- Adicionar build/test Windows real quando houver runner, sem declarar MSVC ou
  Windows ARM64 validados a partir de analise estatica.

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

### Entregas da 0.9.6

- Importacao SSA XLSX com merge seletivo, politica temporal fail-closed e full
  rescan all-or-nothing.
- Catalogo de 77 campos, 184 labels de origem e 168 aliases normalizados, todas
  as worksheets, datas Excel e formula somente com valor em cache.
- Corpus processado sequencialmente sem cap global de 64, streaming em blocos
  de 1.000 linhas, lock entre processos e journal de consolidacao retomavel.
- Supervisor publica `Drained` somente depois de starts e arvores zerarem,
  inclusive quando o lider sai durante startup.
- Conversor LibreOffice preservado como componente isolado, fora do fluxo SSA.

### Entregas da 0.9.7

- SAM validado de manifesto ate SQLite, com lote atomico entre setores e
  rejeicao fail-closed de resposta potencialmente truncada no limite 200.
- Importacao explicita de derivadas em CSV, TXT, TSV, XLSX e XLSM; XLS legado
  somente por selecao e preflight visivel, fora do fluxo SSA.
- Ordem prioritaria de setores e responsaveis centralizada numa unica constante
  usada por display e SQL.
- Seletor de colunas ancorado ao acionador real, reparentado ao overlay e
  recalculado com clamp durante resize visivel.

### Entregas da 0.9.9

- Staging compara identidade, tamanho e mtime da fonte antes de publicar a
  copia, rejeitando substituicoes TOCTOU sem alterar o destino.
- Merge terminal preserva indicadores de execucao, planejamento, responsaveis
  e descricao sem permitir downgrade de STE ou SCA.
- Mapper trata emissao como data primaria e issue como fallback por linha.
- Icone `app_icon` e desktop entry sao arquivos rastreados e entram no bundle
  macOS, nos pacotes Linux/Windows e nos artefatos de distribuicao.
- A sequencia de versoes permanece 0.9.x; 1.0 exige autorizacao explicita.

### Entregas da 0.9.10

- Rescan publica banco e journal duravel antes de mover fontes, retoma
  consolidacao parcial e preserva diagnostico de cancelamento/falha.
- Staging classifica substituicao ou desaparecimento da fonte depois do
  snapshot inicial como alteracao detectada, inclusive com mesmo tamanho e
  mtime.
- Locks de importacao e capability do writer impedem mutacao SQLite fora do
  workflow autorizado sem introduzir lock interno recursivo.
- Schema persistido permanece ASCII canonico; aliases PT/ES/EN ficam no
  catalogo de headers e labels visuais sairam de `domain::ColumnCatalog`.
- GUI e CLI enviam cabecalhos CSV explicitamente; infra valida cardinalidade
  e nao decide metadata visual.
- Supervisor N3 permanece fail-closed, `FailedToStop` nao destroi `QProcess`
  vivo e novos starts so retornam depois de drain comprovado.
- Historico circular de 30 logs/erros e arquivos rotativos limitados tornam
  diagnosticos completos selecionaveis e copiaveis.
- Menus, popup e grafo ganharam contratos de efeito, pointer, bounds,
  determinismo, exportacao e teclado; cobertura pointer por familia continua
  pendente onde registrado no backlog.
- Resumo `qtd_derivadas` e prefetch possuem medidas locais; instrumentacao
  isolada de CPU/idle e validacao externa multiplataforma continuam pendentes.

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

- `Import external XLSX`: done. `SpreadsheetImportWorkflowPort` is wired in
  `DesktopMainViewModelFactory`; QML invokes it through the Importacao menu and
  FileDialog. Legacy XLS remains isolated from the SSA workflow.
- `Rescan/update data`: done. `IImportWorkflowPort::rescan` wired; QML invokes
  via Importacao menu, Manutencao menu, and toolbar button.
- `Update derivadas`: the current C++ action only cleans orphan references.
  Explicit CSV/TXT/TSV/XLSX/XLSM import is now present as a separate action.
  Legacy XLS requires explicit selection and converter preflight. Graph view,
  Mermaid copy, and PNG export remain present.

### SAM depois da 0.9.2

- Avaliar XPath/Playwright somente se o contrato REST deixar de atender o
  fluxo operacional.
- Definir contrato separado para credenciais com Keychain, Credential Manager
  e Secret Service antes de aceitar escopos com usuario, senha ou token.
- Avaliar paginacao do `scrap_report`. A 0.9.7 rejeita exatamente 200 registros
  por setor como potencial truncamento e nao publica esse lote.
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
