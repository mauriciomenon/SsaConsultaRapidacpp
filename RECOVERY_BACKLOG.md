# Recovery Backlog

## Checkpoint causal de copia/staging - 2026-07-18

- [RESOLVED-LOCAL] [IMPORT-STAGING-FIRST-CHUNK-CAUSAL] `79f8200` remove sete
  polls de `.part` em `SpreadsheetImportWorkflowPortTests.cpp`. O hook opcional
  de primeira escrita e injetado apenas pelos contratos e atravessa
  `SpreadsheetImportWorkflowPort` ate `CancelableFileCopy`; uso normal sem hook
  preserva o fluxo de producao.
- A seam permite provocar deterministicamente cancelamento, alteracao de fonte,
  falha de staging e cleanup sem thread auxiliar, deadline ou leitura repetida
  de diretorio. Excecao do hook vira `Failed`/`CleanupFailed` depois de fechar e
  tentar remover o temporario; nao ha swallow, retry ou fallback silencioso.
- O RED `1/1` confirmou a excecao escapando. Dois reviews Terra ultra apontaram
  P2 no caminho de cleanup; apos a correcao, review final `SEM FINDINGS`.
- Gates: build `ssa_integration_tests`; CTest focado `8/8` em `0.80 s`; suite
  workflow `174/174` em `8.65 s`. Diff, formato, cppcheck, detect-secrets e
  Semgrep limpos; clang-tidy sem aviso novo no diff. Hooks staged passaram
  Gitleaks e TruffleHog. Bitbucket confirma `79f8200`; GitLab `origin` segue
  OAuth `invalid_grant`.
- Sem credito novo: plano `99.0/100`, divida nova `13/14 = 92.9%`, legado
  placeholder `0.0/100`, fila operacional `5/8 = 62.5%`.

Proxima atividade unica: trocar polling de processo do conversor legacy por um
handshake causal que prove inicio e cancelamento sem timeout como sucesso.

## Polling de staging e corpus lock removido - 2026-07-18

- [RESOLVED-LOCAL] [IMPORT-STAGING-CORPUS-CAUSAL] `1f99a90` remove tres
  precondicoes temporais de `SpreadsheetImportWorkflowPortTests.cpp`, sem
  alterar producao, schema ou timeout SQLite. Cada teste injeta um semaforo
  novo e consome o primeiro `writerBusyEntered` apos staging real.
- Os contratos preservam a evidencia externa: arquivo staged antes do
  cancelamento, cleanup owned com falha visivel de permissao e exclusao do
  `QLockFile` concorrente enquanto o rescan esta bloqueado.
- Review Terra ultra apontou P1 porque `snapshotLocked` nunca seria emitido no
  preflight de #456. A correcao usa `writerBusyEntered`; review final:
  `SEM FINDINGS`.
- Gates: build de `ssa_integration_tests`; CTest focado `3/3` em `0.48 s` e
  suite workflow `174/174` em `7.72 s`. Diff, formato, detect-secrets e
  Semgrep limpos; hooks staged passaram Gitleaks e TruffleHog. Bitbucket
  confirma `1f99a90`; GitLab `origin` permanece OAuth `invalid_grant`.
- Sem credito novo: plano `99.0/100`, divida nova `13/14 = 92.9%`, legado
  placeholder `0.0/100`, fila operacional `5/8 = 62.5%`.

Proxima atividade unica: remover polling de staging direto somente com uma seam
de evento que prove copia ou conversao em andamento.

## Sinais causais de contencao SQLite na importacao - 2026-07-18

- [RESOLVED-LOCAL] [IMPORT-RESCAN-CAUSAL-BUSY-SIGNALS] `f62ea53` elimina
  precondicoes temporais nos contratos de writer, cleanup de journal, preflight,
  publication e cancelamento do rescan. `writerBusyEntered` prova callback busy
  do writer; `snapshotLocked` prova contencao da publication pelo SQLite Backup.
- A seam e opcional e nula em producao. `WriteSession::Storage` retem o
  `shared_ptr` antes do handler. O backup usa um `atomic_flag` compartilhado
  pelo handler `SQLITE_BUSY` e pelo fallback `SQLITE_LOCKED`, sem permit velho.
- Review Terra ultra encontrou P2 de permit residual no cleanup e P2 de dupla
  publicacao do semaforo; ambos corrigidos. Review final: `SEM FINDINGS`.
- Gates: build afetado; CTest causal `7/7` em `0.56 s`; workflow `174/174` em
  `7.96 s`; familia SQLite Repository `31/31` em `0.72 s`. Diff, formato,
  cppcheck, detect-secrets e Semgrep limpos; hooks staged passaram Gitleaks e
  TruffleHog. Bitbucket confirma `f62ea53`; GitLab `origin` continua OAuth
  `invalid_grant`.
- Sem credito novo: plano `99.0/100`, divida nova `13/14 = 92.9%`, legado
  placeholder `0.0/100`, fila operacional `5/8 = 62.5%`.

Proxima atividade unica: trocar polling de staging e corpus lock por sinais
causais de filesystem e lock nos contratos de importacao restantes.

## Reconciliacao do macro report - 2026-07-18

- [RESOLVED-HISTORICAL] [P4] O macro report atual ja usa
  `COUNT(DISTINCT "ssa_number")` via `ActivityAnalyticsSqlBuilder`; o repositorio
  so le as linhas agrupadas. O antigo `map<ReportKey,set<string>>` nao existe no
  HEAD e nao ha migracao pendente.
- Contratos locais `3/3` em `0.13 s` provam SQL compilado, filtros completos e
  resultado agrupado. Nenhum benchmark novo recebeu credito: o finding de
  gargalo foi refutado, mas escala futura do report ainda nao foi medida.
- Contadores sem mudanca: plano `99.0/100`, divida nova `13/14 = 92.9%`, legado
  placeholder `0.0/100`, fila operacional `5/8 = 62.5%`.

Proxima atividade unica: substituir precondicoes temporais de `sqliteBusyWait`
nos contratos de importacao/rescan por sinais causais da primeira contencao SQLite.

## Determinismo causal do repositorio SQLite - 2026-07-18

- [RESOLVED-LOCAL] `28700b3` substitui o polling de lock e waits arbitrarios de
  `10/50 ms` por sinais causais: lock de escrita adquirido, callback busy e
  callback progress SQLite. A seam e opcional, com `shared_ptr` de semaforo de
  contagem; producao usa todos os sinais nulos.
- Review Terra encontrou P2 de lifetime/overflow dos semaforos; a correcao
  passou a reter ownership. O gate encontrou P1 de incompatibilidade de tipo,
  corrigido por alias unico dos handlers. Review final Terra ultra: `SEM FINDINGS`.
- Gates locais limpos: diff, clang-format, cppcheck, detect-secrets e Semgrep
  `62` regras/zero finding; `clang-tidy` so um aviso preexistente. Build passou;
  familia SQLite Repository `31/31` em `0.68 s`. Hooks staged passaram Gitleaks
  e TruffleHog.
- Bitbucket confirma `28700b3`; GitLab `origin` segue OAuth `invalid_grant`;
  local `71` commits a frente. Contadores sem inflacao: plano `99.0/100`, divida
  nova `13/14 = 92.9%`, legado placeholder `0.0/100`, fila `5/8 = 62.5%`.

Proxima atividade unica: medir a equivalencia e o custo do macro report
`COUNT(DISTINCT)` antes de remover a agregacao em memoria.

## Indice status-last de producao - 2026-07-18

- [RESOLVED-LOCAL] `4396e1c` instala no writer o indice de expressao
  `idx_<table>_status_last_numero_ssa_desc`, com a mesma expressao do query
  builder e somente em tabelas que possuem `numero_ssa` e `situacao`.
  Bancos existentes recebem o indice na proxima escrita, sem alterar
  `user_version` ou fazer migracao em leitura.
- A integracao prova DDL e plano exatos, recriacao apos indice ausente, tabela
  customizada legacy e colisao homonima fail-closed que nao remove o indice de
  outra tabela. O primeiro review Terra ultra encontrou dois P2 (ownership e
  fixture legacy), ambos corrigidos; o review final retornou `SEM FINDINGS`.
- Gates locais limpos: diff, clang-format, cppcheck, detect-secrets e Semgrep
  com `62` regras/zero finding; `clang-tidy` so avisos preexistentes fora do
  diff. Build afetado passou; CTest direto `38/38` em `1.14 s`.
- Publicacao: Bitbucket confirmado em `4396e1c`; GitLab `origin` bloqueado por
  OAuth `invalid_grant`; local `69` commits a frente de `origin/master`.
- Sem credito novo: plano `99.0/100`, divida nova `13/14 = 92.9%`, legado
  placeholder `0.0/100` e fila operacional `5/8 = 62.5%`.

Proxima atividade unica: remover polling temporal de `SqliteRepositoryTests`
com sinais causais ja presentes em lock, busy handler e progress handler.

## Benchmark SQLite status-last - 2026-07-18

- `1d950e3` entrega somente o harness isolado: fixture de `250000` linhas,
  `30` amostras e JSON local nao versionado em
  `build/dev/sqlite-status-last-benchmark-30.json`.
- Baseline: plano com `TEMP B-TREE`; indexado: usa
  `idx_ssa_table_status_last_numero_ssa_desc`, sem `TEMP B-TREE`; vetores de
  resultados identicos. Wall p50/p95: `41.260958/45.094667 ms` versus
  `0.0235/0.030125 ms`. CPU p50/p95: `41.184/44.742 ms` versus
  `0.024/0.031 ms`.
- Gates estaticos limpos; smokes `2/2` em `0.37 s`; Terra ultra final:
  `SEM FINDINGS`. O escopo nao prova filtros arbitrarios ou GUI completa e
  ainda nao instala indice de producao.
- Publicacao historica: Bitbucket `0/0` em `1d950e3`; GitLab `origin` com
  OAuth `invalid_grant`; local `67` commits a frente de `origin/master`.
- Contadores sem mudanca: plano `99.0/100`, divida nova `13/14 = 92.9%`,
  legado placeholder `0.0/100` e fila operacional `5/8 = 62.5%`.

Proxima atividade unica: RED de integracao e indice DESC de expressao no writer.

## Cobertura sintetica de URL Windows/UNC - 2026-07-18

- HEAD anterior: `e76e722`. O commit `bca795a` entrega somente em
  `AdvancedPopupQmlTest.cpp` um contrato para `file:///C:/SSA/graph.png` e
  `file://server/share/graph.png`: `DerivadasGraphModel::localFilePath()`
  delega exatamente para `QUrl::toLocalFile()`. Na publicacao de `bca795a`,
  Bitbucket foi confirmado com divergencia `0/0`; GitLab `origin` falhou por
  OAuth `invalid_grant`, e o HEAD local estava 65 commits a frente de
  `origin/master`.
- Validacao local: `ssa_qml_advanced_popup_tests` `1/1` em `2.16 s`;
  `git diff --check`, clang-format e detect-secrets passaram; Semgrep executou
  53 regras com zero finding. Review Terra ultra terminou sem findings.
- Sem producao e sem credito: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`.
- Risco residual: cobertura sintetica nao prova SMB, `CreateFileW`, reparse
  points, handles de diretorio ou gravacao em share. Proxima atividade unica:
  prova Windows/UNC real para importacao e grafo. Profiling do prefetch continua
  bloqueado.

## Pendente (fora do escopo desta trilha)

### Sequencia ativa preservada

1. Completar profiling valido do prefetch; harness, 30 amostras e relatorio
   isolado ja estao resolvidos no working tree. `xctrace` nao possui `Time
   Profiler` nesta instalacao e `CPU Counters` falhou com `DTServiceHub` e
   politica do kernel; nenhum credito de profiling foi atribuido.
2. Validar `ImportFileConsolidator` e URL de grafo em Windows/UNC real; o split
   local entre staging owned pre-commit e consolidacao post-commit ja esta
   implementado.
3. Revalidar handles, PowerShell e packaging em plataformas reais.

### Controle operacional auditavel

Fila de fechamento principal: `5/8 = 62.5%`.

| Pacote | Estado | Efeito no contador |
| --- | --- | --- |
| Busy wait integral de importacao | Resolvido em `7291082` | `1/8` |
| Gate transacional de schema SQLite | Resolvido em `b2369ac` | `2/8` |
| `TEST-DETERMINISM-DELTA` | Resolvido em `785c73e..bcb7980` | `3/8` |
| `IMPORT-FILENAME-TIMESTAMP-SCAN` | Resolvido em `be20b99` | `4/8` |
| Tooling clang-tidy/Semgrep | Resolvido localmente em `2e2476b` | `5/8` |
| Profiling valido do prefetch | Bloqueado pela ferramenta local | Leva a `6/8` |
| Importacao e grafo Windows/UNC | Pendente externo | Leva a `7/8` |
| Handles, PowerShell e packaging reais | Pendente externo | Leva a `8/8` |

O unico item nao aceito do denominador fixo de 14 e `IMPORT-STAGER-HUB`.
Determinismo e filename elevaram a divida nova para `13/14 = 92.9%`; o item
restante exige Windows/UNC real.
O `0.0/100` legado abaixo e placeholder historico sem denominador, nao medida de
execucao; nao deve ser usado isoladamente em reports futuros.

Contadores auditados: plano original `99.0/100`, divida nova `92.9/100`
(13 de 14 itens enumerados aceitos) e backlog legado `0.0/100`, sem denominador
aceito.
Os creditos recentes pertencem a `TEST-DETERMINISM-DELTA` e
`IMPORT-FILENAME-TIMESTAMP-SCAN`. Findings descobertos depois da lista fixa de
14 itens nao reduzem retroativamente esse percentual.

### Fechamento local de tooling

- HEAD de codigo `2e2476b`; Bitbucket confirmado com divergencia `0/0`. GitLab
  segue bloqueado por OAuth `invalid_grant`; o HEAD local esta 63 commits a
  frente de `origin/master`.
- [RESOLVED-LOCAL] [CLANG-TIDY-MACOS-SYSROOT] `CMAKE_OSX_SYSROOT=macosx` foi
  adicionado aos cinco presets-raiz; `dev-arrow` herda `dev`. O compile database
  registra SDK macOS 26.5. `clang-tidy` direto sem args extras passou em `2.50
  s`; `ssa_infra` compilou 58 passos em `5.55 s`, integracao 160 em `13.19 s` e
  mapper `15/15` em `0.44 s`.
- [REFUTED-LOCAL] [SEMGREP-WIDE-NARROW-TIMEOUT] A alegacao introduzida em
  `63554b9` nao possui IDs, YAML, fixture ou log. Semgrep 1.170 focado no teste
  de importacao de 6,815 linhas passou em `2.92 s`, com duas regras atuais
  elegiveis, zero finding e zero timeout. Nenhuma fixture foi dividida. A
  equivalencia exata ao Semgrep 1.169 pinado no CI continua sem prova porque o
  socket Docker local esta ausente; isso e dependencia externa, nao sucesso.

Proxima atividade unica: prova Windows/UNC em plataforma real para importacao e
grafo. Profiling permanece bloqueado; handles, PowerShell e packaging seguem
dependentes de plataformas reais.

### Fechamento local de determinismo e filename

- HEAD de codigo `be20b99`; Bitbucket confirmado com divergencia `0/0`. GitLab
  segue bloqueado por OAuth `invalid_grant`; antes do commit documental, o HEAD
  local estava 61 commits a frente de `origin/master`.
- [RESOLVED] [TEST-DETERMINISM-DELTA] Os commits de `785c73e` ate `bcb7980`
  substituem esperas artificiais por barreiras causais, sinais, clock injetado,
  hook post-commit e gates terminais. A matriz focada passou `26/26` em
  `28.38 s`; o mapper passou `30/30` em `2.21 s` e preservou a janela temporal
  deliberada. Waits restantes observam filesystem, processos, SQLite, timers
  ou cancelamento reais. Nenhum timeout recebeu credito.
- [RESOLVED-BENCHMARK] [IMPORT-FILENAME-TIMESTAMP-SCAN] `be20b99` adiciona o
  harness formal. Target e build passaram; o gate passou `3/3` em `0.41 s` e o
  recheck `3/3` em `0.13 s`. Foram 30 processos, 12,000 parses e `2.00 s`, com
  p95 de `783.3 ns` para ISO no inicio e maximo adversarial de
  `178269.15 ns`, ou `0.178 ms`. O parser e O(n) e nenhuma otimizacao foi
  justificada.
- O CTest sequencial passou `595/595` em `66.86 s` antes da inclusao do novo
  smoke. O registro configurado atual e 596; o full nao inclui o teste 596.
- Security ampla: Semgrep `509` arquivos, `24` regras e zero finding; Gitleaks
  `1.42 GB` e zero leak; TruffleHog `9,226` chunks e zero segredo.

Proxima atividade unica: canonizar clang-tidy macOS com sysroot e reproduzir
IDs Semgrep. O profiling continua bloqueado pela ferramenta; depois seguem as
provas externas Windows/UNC, handles, PowerShell e packaging.

Fechamento de 2026-07-18: HEAD de codigo `b2369ac`, precedido por `7291082`,
`0025d44` e `7f396b0`. `610fbf3` fecha o recheck WAL por snapshot. Em
`7f396b0`, os targets afetados compilaram; conversor passou 20/20, SAM passou
20/20 suites completas, Details passou 10/10 e CurrentWeek passou 20/20 sem
patch. O CTest sequencial final passou `588/588` em `78.30 s`. Semgrep amplo
escaneou 508 arquivos sem finding; Gitleaks escaneou 1.42 GB sem leak e
TruffleHog escaneou 9,029 chunks sem segredo. No gate atual, o CTest final
passou `595/595` em `75.81 s`; Semgrep, Gitleaks e TruffleHog ficaram sem
finding. Bitbucket foi confirmado em `b2369ac`; o push GitLab continua
bloqueado por OAuth `invalid_grant`.

- [RESOLVED] [SQLITE-RESCAN-WAL-SNAPSHOT] `610fbf3` prova que um leitor WAL em
  transacao preserva o snapshot antigo durante a publicacao e observa o novo
  somente apos encerrar e iniciar outra transacao.
- [RESOLVED] [SQLITE-BACKUP-BUSY-RETRY] `7291082` provou que o busy handler do
  Backup API ja aguarda contencao temporaria no preflight e na publicacao e
  corrigiu o gap real: recovery lookup e journal cleanup ignoravam o prazo do
  request e usavam 250 ms fixos. Os tres contratos passaram 30 execucoes e a
  familia ampliada passou `15/15` em `2.67 s`, preservando fail-closed,
  cancelamento, integridade e fontes.
- [RESOLVED] [SQLITE-SCHEMA-VERSION-GATE] `b2369ac` aceita legado zero e a
  versao atual, rejeita versoes futuras antes de DDL/DML, lookup, cleanup ou
  movimento de recovery e carimba `schemaVersion()==1` dentro da write
  transaction. Rollback e crash preservam zero. O validator usa snapshot
  explicito. Contratos principais passaram `80/80`, cleanup direto `30/30`,
  gate amplo `45/45` e CTest final `595/595` em `75.81 s`. Sol encontrou e
  fechou 1 P1 e 2 P2; o recheck final excedeu a janela e nao recebeu credito.

Ultimo fechamento: commit `e0b5401`, build e CTest da suite de painel `1/1`
passaram em `2.96 s`. O polling de `activeFilterEntries()` continua pendente
por depender de timer; nao foi removido por inferencia.

Fechamento seguinte: commit `41ecf36`, duas leituras sincronas de `textFilter()`
foram convertidas para `QCOMPARE`; a suite passou `1/1` em `5.74 s`. O item
amplo `TEST-DETERMINISM-DELTA` continua pendente e sem credito adicional.

Fechamento atual: commit `c95a757`, a leitura sincronica de `sector->quickSector()`
foi convertida para `QCOMPARE`; a suite passou `1/1` em `2.88 s`. Polls de
resumos baseados em timer permanecem preservados.

Gate amplo: CTest sequencial foi confirmado em duas etapas, `1-433` aprovados
na primeira e `434-452` aprovados `19/19` em `28.78 s`; total `452/452`.

Fechamento do workflow hub: commits `14485a9` e `627a075` extraem adaptacao
SAM e importacao XLSX por chunks para resultados internos tipados. O CTest
sequencial passou `453/453` em `64.05 s`; Bitbucket foi confirmado em
`627a075` e GitLab continua bloqueado por OAuth `invalid_grant`.

Fechamento local do stager hub: commit `55c6bd8` reduz
`ImportFileStager.cpp` de 1364 para 634 linhas e concentra moves em
`ImportFileConsolidator.cpp`. Gate focado `11/11` em `0.36 s` e suite completa
`453/453` em `65.75 s`. Bitbucket confirmado; Windows/UNC real ainda pendente.

Determinismo: a tentativa `7ec88ae` foi rejeitada: `QTRY` bombeou o event loop
e publicou a falha tardia antes de `cancel()`, quebrando a janela preterminal.
Commit `9b960ed` restaurou os loops deliberados; suite passou `1/1` em `0.75 s`.
O item amplo continua sem credito binario.

Reteste: a faixa final `449-452` passou `4/4` em `12.24 s`, e o teste 434
passou novamente apos a restauracao do polling preterminal.

- [RESOLVED-WT] [WIDTHS-BY-KEY] O catalogo de presentation agora pareia cada
  largura default com sua key canonica. Os 85 valores foram preservados, o
  lookup nao depende mais da ordem do domain e o teste focado cobre todas as
  entradas e sentinelas.

## Reauditoria Cursor GLM e OpenCode (2026-07-17)

Matriz completa de acertos, adicoes, erros e ordem de execucao:
`docs/plans/2026-07-17-v0.9.10-audit-handoff.md`.

- [RESOLVED-WT-P0] [FILTER-DIFFERENT-SEMANTICS] `TextFilterToken` chamava o operador de
  `Different` e serializa `!valor`, mas `SearchParser` compila esse token como
  `NOT LIKE '%VALOR%'`. O not-equals exato ja existe como `!=valor`. Corrigir a
  fronteira de serializacao ou parsing sem mudar a negacao da busca geral.
  Aceite: um dataset com `ADM`, `PRE-ADM` e `APV` deve provar que Different
  remove apenas `ADM`, enquanto not-contains continua removendo ambos os textos
  que contem `ADM`. Arquivos previstos: `src/domain/TextFilterToken.cpp`,
  `tests/unit/TextFilterTokenTests.cpp`, `tests/unit/SearchParserTests.cpp` e
  `tests/integration/SqliteRepositoryTests.cpp`. O working tree serializa
  `!=valor`, le `!valor` legado e passou contratos unitarios, SQLite e smoke.

- [RESOLVED-WT-P0] [DISTINCT-ADVANCED-TEXT-FILTERS]
  `SqlQueryBuilder::buildDistinctValues()` incorpora `columnFilters`, mas omite
  `advancedFilters.textFilters`, apesar de `build()` combinar os dois mapas.
  Definir explicitamente se o filtro da propria coluna alvo deve ser mantido ou
  removido; os filtros das demais colunas devem restringir as opcoes exibidas.
  Aceite: teste unitario da SQL/bindings e teste SQLite onde o filtro avancado
  altera o conjunto distinct. Arquivos previstos: `src/query/SqlQueryBuilder.cpp`,
  `tests/unit/SqlQueryBuilderTests.cpp` e `tests/integration/SqliteRepositoryTests.cpp`.
  O working tree aplica os filtros avancados remanescentes; self-filter e
  removido somente pelo request builder da coluna alvo.

- [RESOLVED-WT-P0] [DESKTOP-LOG-HANDLER-IO-LOCK] `DesktopLogSink::messageHandler()` segurava
  `handlerMutex_` ao chamar `record()`, que formata a mensagem, abre/grava/rotaciona
  arquivo e agenda atualizacao do model. Isso serializa todos os produtores Qt
  pela latencia de filesystem e amplia risco de reentrancia. O fix deve obter
  ownership/lifetime estavel sob lock e fazer I/O fora dele, ou usar uma fila
  estritamente limitada com shutdown deterministico. Aceite: append lento nao
  bloqueia instalacao/desinstalacao indefinidamente, logging concorrente nao usa
  sink destruido e falha de writer continua visivel sem recursao. Arquivos
  previstos: `app/desktop/DesktopLogSink.{h,cpp}` e testes dedicados. O working
  tree usa estado compartilhado, reserva in-flight e drain; I/O, invoke Qt e
  handler anterior ficam fora do mutex global. Risco residual Minor: callback
  ja despachado ao trampoline, mas ainda sem reserva, pode perder uma linha no
  exato uninstall sem causar UAF ou deadlock.

- [RESOLVED] [IMPORT-WORKFLOW-HUB] Os commits `14485a9` e `627a075` extraem
  leitura/adaptacao SAM e streaming/mapping/escrita XLSX por chunks para
  operacoes internas com resultados tipados. A mesma `WriteSession` continua
  sob o orquestrador; catches, rollback, journal e consolidacao nao mudaram.
  O reviewer Sol confirmou a preservacao de contadores parciais em falha
  tardia, e o CTest sequencial passou `453/453` em `64.05 s`.

- [IMPLEMENTED-LOCAL] [IMPORT-STAGER-HUB] O commit `55c6bd8` separa staging
  owned pre-commit de consolidacao mutavel post-commit. `ImportPathValidation`
  compartilha as validacoes de root/filename, e identidade continua em
  `CancelableFileCopy`. POSIX passou 11/11 focados e 453/453 completos. Falta
  executar os mesmos contratos em Windows/UNC real para aceite binario.

- [RESOLVED-WT] [COLUMN-WIDTH-KEY-PAIRING] As 85 entradas de `kDefaultWidths`
  agora sao pares `{key, width}` em presentation. O lookup e o teste focado
  nao dependem da posicao em `ColumnCatalog::all()`; nenhum layout foi alterado.

- [RESOLVED-WT-P0] [ROTATING-LOG-UTF8-TRUNCATION] `RotatingLogWriter::append()` usava
  `std::string::resize()` por bytes e pode cortar um code point UTF-8. Truncar na
  ultima fronteira valida que caiba com o sufixo. Adicionar payload maior que o
  cap com caracteres multibyte e validar UTF-8, tamanho e marcador. O finding de
  rename ignorado foi refutado: a overload atual lanca em erro. O working tree
  recua ate uma fronteira UTF-8 valida e cobre limite minimo, ASCII, corte
  exato, code point de quatro bytes, tamanho e sufixo.

- [RESOLVED-BENCHMARK] [SQLITE-IMPORT-NORMALIZE-FULL-SCAN]
  O harness mede `first-pass` e `idempotent-reopen` sem chamar o segundo caso de
  normalizacao warm. Em 30 amostras por cenario com 250 mil linhas, legacy
  atualizou exatamente 250 mil registros com wall p50/p95
  `1959.458/3258.614 ms`, CPU `1916.130/2816.048 ms` e RSS adicional p95
  `98,320,384` bytes. Reabertura idempotente teve wall p50/p95 maximo
  `1.883/3.894 ms`, zero updates e RSS p95 abaixo de 1.36 MB. Integridade,
  canonicalidade e indices foram verificados em toda amostra; nenhuma mudanca
  de schema foi justificada pelos numeros atuais.

- [RESOLVED-BENCHMARK] [SQLITE-READ-CONNECTION-CHURN] O repository abre
  conexao, busy/progress handlers e statements por read, isolando threads e
  tokens. Em 30 processos sobre 250 mil linhas, first read wall p50/p95 foi
  `6.948/12.897 ms`; open/read/close repetido, 3,000 observacoes, ficou em
  `1.390/3.296 ms`; RSS repeated p95 foi `16,384` bytes. As 2,400 chamadas
  concorrentes de `page()` completaram sem falha ou cancelamento, com wall
  agregado por read `157.501/219.458 ms`. O cache de paginas do SO nao foi
  controlado. Pool ou handle persistente nao foi justificado; handlers e
  cancelamento por operacao permanecem preservados.

- [RESOLVED] [SQLITE-RESCAN-OPEN-HANDLE-PUBLICATION] `2dd9aec` publica o
  working database pelo SQLite Backup API no handle de destino, sem rename ou
  unlink. Busy wait e cancelamento sao limitados pelo request; `SQLITE_DONE`
  vence stop tardio porque o commit ja ocorreu. Os 3 REDs passaram `3/3` em
  `0.22 s`, e o gate ampliado passou `7/7` em `0.35 s`, cobrindo leitor aberto,
  fail-closed sob lock, integridade, sidecars, full rescan e cancelamento.
  O recheck WAL foi fechado em `610fbf3`; resta validacao Windows real, sem
  reabrir o finding POSIX.

- [RESOLVED] [SQLITE-PAGE-FILTER-CONCURRENT-LATENCY] O benchmark real
  mediu 2,400 chamadas de `page()` com filtro exato em 250 mil linhas: wall
  p50/p95 `157.501/219.458 ms`; batch 4 x 20 p50/p95
  `3281.940/14109.139 ms`. `EXPLAIN QUERY PLAN` provou a causa: `Equals` em
  `numero_ssa` adiciona `COLLATE NOCASE`, mas o indice e BINARY. No banco real,
  count/rows atuais fizeram scan de 94,879 linhas em `43/71 ms`; sem NOCASE,
  ambos usaram `SEARCH` em `2/<1 ms`. `1cea684` usa igualdade BINARY somente
  para SSA canonica e preserva NOCASE textual. O planner passou em page/count;
  30 processos terminaram 2,400/2,400 reads, zero falha/cancelamento, com
  p50/p95 `0.133/0.229 ms` e batch `2.902/4.241 ms`.

- [RESOLVED] [DERIVED-SUMMARY-MUTEX-IO] `360d18b` limita o mutex a eleicao e
  publicacao de estado. Um unico initializer executa QLockFile e I/O SQLite
  fora do mutex; callers concorrentes usam fallback read-only. Cancelamento e
  excecoes limpam a eleicao e permitem retry imediato. O RED bloqueou o
  fallback; depois do patch, 2/2 passaram, 60 repeticoes ficaram estaveis,
  `13/13` cobriram a familia de derivadas e `8/8` cobriram leitura e
  cancelamento. Sol xhigh e Terra high terminaram em GO.

- [RESOLVED-WT] [SQL-DERIVADAS-BUILDER-BOUNDARY] `derivadasDiretas()` agora
  recebe SQL e binding de `SqlQueryBuilder::buildDirectDerivations()`. Colunas,
  `IS NOT NULL`, ordenacao e contrato de cancelamento foram preservados.

- [RESOLVED-WT] [ADVANCED-FILTER-ROW-LOOKUP] `AdvancedTextFilterRowModelFactory`
  usa diretamente as keys canonicas de `advancedFilterKeys()`, removendo nove
  buscas e o ramo nulo redundante sem alterar ordem ou labels.

- [RESOLVED-WT] [COLUMN-FILTER-ROW-LOOKUP] `ColumnFilterViewModel` usa
  diretamente as keys de `orderedFilterColumnKeys()`, removendo lookup e null
  branch redundantes sem alterar ordem, labels ou valores iniciais.

- [VALIDATED-NO-CREDIT] [FILTER-PANEL-POLLING] O teste de quick sector agora usa
  `QCOMPARE` direto porque o fluxo e sincrono no mesmo thread. O `QTRY` e o
  timeout fixo foram removidos sem alterar o contrato de producao.

- [RESOLVED-WT] [BROWSE-SECTOR-CAST-INVARIANT] `BrowseRequestCoordinator` agora
  usa a API publica `FilterPanelViewModel::excludeScaSesSte()` diretamente. O
  include concreto, o cast e a dereferencia sem check foram removidos; o teste
  focado de exclusao passou 1/1.

- [RESOLVED-WT] [IMPORT-COMPLETENESS-LOOKUP] `completenessScore()` usa o
  `value` ja iterado em vez de repetir `map::find` e alocacao de key. Os casos
  de richness e metadata passaram 2/2.

- [VALIDATED-NO-CREDIT] [IMPORT-FILENAME-TIMESTAMP-SCAN] Foi adicionado caso
  adversarial com 4096 digitos de ruido antes de timestamp valido. O teste
  passou `1/1` em `0.02 s`; o parser nao foi alterado e nenhum credito de
  performance foi atribuido sem benchmark temporal formal.

- [TEST-DEBT] [TEST-DETERMINISM-DELTA] Pendencias confirmadas, sem regressao
  funcional demonstrada: `sleep_for(1050ms)` para backoff do resumo derivado;
  polling SQLite de 2ms; duas esperas negativas de 400ms; varios waits de 1/5/10
  ms; `/tmp` em casos que precisam ser separados entre path apenas simulado e
  I/O real; contador static de diretorio em `SqliteDatabaseValidatorTests`; e
  screenshot que retorna apenas false no timeout. Migrar por suite para clock
  injetavel, `QTemporaryDir`, condition-based wait e diagnosticos QtTest, sem
  alterar testes que modelam delay/cancelamento deliberadamente.

### Findings refutados ou dependentes de produto

- [REFUTED] [A6-1-DOMAIN-WIDTH] Nao existe `defaultWidth == 98` em
  `DomainTests.cpp`; o valor citado esta em teste legitimo de round-trip de
  preferences.
- [REFUTED] [B22-4-1769] O corpus de 1769 XLSX e contrato de inventory, distinto
  do limite de selecao externa.
- [REFUTED] [CONDITIONAL-DEPTH-C1-C3] `positionalFamily` e
  `removeActiveFilter` usam dispatch flat. Contagem de branches nao prova
  lentidao; tabela/strategy adicionaria estrutura sem ganho demonstrado.
- [REFUTED] [WORKFLOW-SERVICE-COUPLING] Presentation depender do facade
  application `SsaWorkflowService` respeita a arquitetura. Outro port seria
  duplicacao sem consumidor independente.
- [PRODUCT] [SEARCH-PYTHON-N6-N8] C++ preserva hifen inicial literal e usa
  SafePattern restrito para `~`, ambos cobertos por testes. Alinhar com negacao,
  fallback mode ou regex Python exige decisao de produto e seguranca.
- [OBSOLETE] [QFUTUREWATCHER-NONTRIVIAL] Os fluxos principais usam
  `QFutureWatcher<void>` e estado compartilhado sincronizado apos o fix TSan.
  Auditar teardown separadamente, sem reabrir o finding de payload Qt ja fechado.

- [MEASURED-NO-CREDIT] [QT-ROLENAMES-CACHE] A busca encontrou cinco
  implementacoes de `roleNames()` e apenas chamadas explicitas nos testes;
  nenhum hot path de producao foi demonstrado. Cache permanece deferido, sem
  credito, ate existir medicao de runtime que justifique custo de memoria.

- [LOW] [MINIZ-CMAKE-DEPRECATION] O CMake do source cache do miniz declara
  compatibilidade anterior a 3.10 e gera warning em CMake recente. Nao editar
  `.deps-cache/miniz-src/CMakeLists.txt`: o diretorio e gerado e ignorado. Tratar
  somente ao atualizar o pin do miniz, depois de validar build e pacote em todas
  as plataformas suportadas.

- [LOW] [POWERSHELL-ANALYZER-BASELINE] PSScriptAnalyzer reporta seis usos
  preexistentes de `Write-Host` e duas funcoes de pacote sem `ShouldProcess` nos
  scripts Windows. Nenhum arquivo PowerShell mudou na 0.9.5. Tratar em slice
  Windows dedicado, com validacao real em Windows antes de alterar a saida dos
  scripts ou o comportamento de links de artefatos.

- [PENDING] Implementar sincronizacao completa de derivadas com regras de negocio e fonte externa, incluindo modelagem de grafo/fluxo derivado.
- [RESOLVED-v0.9.7] SAM foi validado ate SQLite com adapter do schema real,
  contagem de manifesto e rejeicao fail-closed no limite potencialmente
  truncado de 200 registros.
- [RESOLVED-v0.9.7] Importacao explicita de derivadas foi separada da limpeza
  de orfas e aceita CSV, TXT, TSV, XLSX e XLSM. XLS permanece sob selecao e
  preflight explicitos do conversor legado.
- [RESOLVED-WT] [GUI-MENUS] Cinco contratos dedicados usam pointer real e
  provam efeito terminal: banco, exportacao, importacao XLSX, manutencao e
  copia de grafo SVG pelo menu de celula. O target help/about passou offscreen;
  nenhum QML de producao ou layout foi alterado. Falta commit/prova externa.
- [RESOLVED-WT] [GUI-POPUP-RECONCILIATION] Smokes 1180x940 e 1580x940 passaram
  2/2. As imagens mostram ancoragem, clamp lateral/vertical e viewport rolavel
  dentro da janela. Nenhum defeito reproduzido justificou mudanca de layout.
- [PARTIAL-EXTERNAL] [IMPORT-CONSOLIDATION-TOCTOU] A consolidacao rejeita
  diretorios symlink e ja vincula validacao/movimento a handles de diretorio,
  com rename no-replace ancorado em POSIX e handles em Windows. A implementacao
  local esta presente; faltam prova externa Windows e revalidacao de plataforma,
  sem reabrir schema, journal ou ownership.
- [LOW] [SAM-LIMIT-200] Cada rodada REST solicita no maximo 200 registros por
  setor e a 0.9.7 rejeita exatamente 200 como potencial truncamento. Avaliar
  paginacao do `scrap_report` antes de liberar setores acima desse volume.
- [LOW] [SAM-SINGLE-FLIGHT-PROCESS] O single-flight da atualizacao SAM vale por
  instancia da aplicacao. Duas instancias podem executar atualizacoes ao mesmo
  tempo. Coordenacao entre processos fica adiada ate existir necessidade
  operacional comprovada.

## Varredura de codigo (junho 2026) - pendentes

### Performance estrutural (deferido - exige migration/schema)
- [RESOLVED-LOCAL] [P2] status-last CASE sort agora usa indice de expressao
  composto DESC, instalado pelo writer na proxima escrita. O benchmark de
  `250000` linhas mostrou plano indexado sem `TEMP B-TREE`; generated column
  continua desnecessaria sem evidencia de gargalo adicional.
- [RESOLVED-HISTORICAL] [P4] Macro report ja usa analytics SQL com
  `COUNT(DISTINCT "ssa_number")`; o antigo agregador em memoria nao existe no
  HEAD. Contratos de SQL, filtros e resultado agrupado passaram `3/3` em `0.13 s`.
- [RESOLVED-LOCAL] [P7] O benchmark `ce90132` mede a formatacao eager de
  pagina preconstruida de `500x12`, com texto, inteiros e datas reais. Em 30
  amostras, wall p50/p95 foi `2.93215/3.3340834 ms` por pagina e CPU p50/p95
  foi `2.9244/3.2649 ms`. O formatter ja roda fora da UI; nao ha evidencia para
  romper `SsaTableDisplayValues`/`displayCache_` por lazy formatting.

### Qualidade / duplicacao
- [RESOLVED] [Q1] As tres copias identicas de `trimCopy` da importacao foram
  consolidadas em `domain/WhitespaceTrim.h`, com prova Qt-free para texto,
  vazio e somente whitespace. Helpers de parser e `string_view` com semanticas
  diferentes permaneceram separados.
- [RESOLVED] [Q4] `SsaImportConflictResolver` usa somente `indexBySsa` para
  localizar linhas aceitas; o antigo `seenNumbers` redundante nao existe no
  HEAD atual.

### Cleanup / low priority
- [RESOLVED-WT] [L-Q2] O finding ficou stale: `emptyFiles` nao existe no
  import atual e `failedFiles` e usado nos caminhos de erro, consolidacao,
  journal e resumo final. Nenhuma remocao de contador e necessaria.
- [RESOLVED-WT] [L-Q3] O finding ficou stale: `AdvancedTextFilterRowModelFactory`
  e `ColumnFilterViewModel` usam keys canonicas diretamente e nao mantem mais
  lookup/null branch redundante. Validacao: busca direcionada sem ocorrencias;
  suites focadas de filtros passaram nos slices correspondentes.
- [RESOLVED] [L-Q4] `tokenOperatorForStorage` foi removida; o enum tipado e
  armazenado diretamente.
- [PRODUCT] [L-A1] `SearchParser` preserva `-valor` como texto literal por
  contrato testado. Python usa hifen como negacao. Decidir paridade antes de
  alterar; nao quebrar busca de conteudo com hifen por inferencia.
- [RESOLVED] [L-A2] A semana 53 agora e aceita somente em anos ISO longos,
  com cobertura para 2020 (valido) e 2021 (invalido).
- [RESOLVED-WT] [L-A3] `LegacySpreadsheetConverter` agora diferencia erro real de
  filesystem de output convertido ausente ou nao regular. O diagnostico nao
  fica vazio quando `is_regular_file` retorna falso sem `error_code`; status,
  cleanup e destino permanecem inalterados. Validacao: build de
  `ssa_integration_tests` e casos 234-237, 4/4.
- [RESOLVED] [L-P1] `columnFilters()` retorna `const&` e
  `ColumnFilterViewModel` nao copia mais o mapa ao atualizar linhas.
  `advancedFilters()` continua por valor porque sintetiza um DTO a partir do
  estado Qt, em vez de expor um mapa armazenado.

## Otimizacao de performance (deferido do ciclo de audit de anti-padroes)

- [MEASURED] [IMPORT-BENCHMARK] Em microbenchmark local com 250000 linhas e 30
  amostras, resumo ficou em 0.035/0.038 ms mediana/p95, coluna omitida em
  0.012/0.013 ms, fallback read-only com `GROUP BY` em 10.168/10.574 ms e cold
  connection em 0.444/0.571 ms. `EXPLAIN QUERY PLAN` confirmou lookup por PK no
  normal e materializacao/temp B-tree no fallback. A remocao direcionada por
  pai reduziu o trigger delete de 100 linhas de 10.143/10.701 ms para
  0.255/0.309 ms em 30 amostras locais.
- [RESOLVED-WT-PARTIAL] [PREFETCH-BENCHMARK] Testes provam prefetch das paginas
  2 e 3, cache hit, invalidacao por fingerprint/generation, cancelamento e
  latest-wins. O novo harness deterministico separa setup, foreground wall/CPU,
  idle wall/CPU e RSS sem sleeps ou polling. O target CMake executou 30/30
  amostras validas e gravou dados brutos e mediana/p95 em
  `build/dev/prefetch-benchmark-30.json`. Credito aceito: harness `1.6`, 30
  amostras `1.2` e relatorio `0.9`. Falta profiling valido por `1.0` ponto; as
  metricas medem o overhead isolado do coordinator com port falso, nao SQLite
  real. Tentativas de profiling com `sample`, `xctrace Time Profiler` e
  `xctrace CPU Counters` nao produziram evidencia aceita: attach foi bloqueado
  por permissao, o template Time Profiler nao existe e CPU Counters falhou no
  `DTServiceHub`.
- [RESOLVED-FAIL-CLOSED] [SUPERVISOR-N3] O reproducer agora passa pela chamada
  real de `retainFailedProcessTree()` com identidade de SO invalida. A falha
  marca `untrackedStopFailure`, `forceStopRequested` e `forceStopFailed` no
  mesmo ponto, bloqueia novos starts e nunca declara drain porque o registry
  ficou vazio. A flag permanece sticky em producao; somente o seam de limpeza
  dos testes a remove. Recuperacao automatica continua proibida sem prova
  positiva multiplataforma de que todos os descendentes terminaram.
- [RESOLVED] [SUPERVISOR-FAILED-TO-STOP] Um reproducer direto de
  `SupervisedProcess::run()` confirmou que o leader `QProcess` permanecia vivo
  quando a parada da arvore falhava, causando `QProcess: Destroyed while
  process is still running`. O leader agora recebe kill e wait limitados antes
  do retorno `FailedToStop`, enquanto a arvore de descendentes permanece no
  registry e o supervisor continua fechado. Drain verificado reabre o
  supervisor. A suite focada passa com `QT_FATAL_WARNINGS=1`, sem reaper thread
  ou ownership cross-thread.

## Riscos estruturais e de tooling registrados

- [RESOLVED] [IMPORT-SCHEMA-BOUNDARY] Labels externos PT/ES/EN continuam
  normalizados por `SsaSpreadsheetHeaderCatalog` para chaves ASCII canonicas.
  O catalogo valida cada destino de alias e `SqliteSsaImportWriter` rejeita
  chaves desconhecidas, nao canonicas ou duplicadas antes de abrir o banco.
  Nao houve migration nem renomeacao de coluna persistida. Os casos focados
  cobrem aliases convergentes e `Desvio #2` persistido como inteiro `2`.
- [RESOLVED] [GUI-LOG-HISTORY] A GUI mantem os 30 eventos mais recentes com
  timestamp, severidade, origem, mensagem e diagnostico completos. O dialog no
  menu Ajuda permite selecionar e copiar um ou todos; o status inferior e
  selecionavel. O desktop persiste mensagens Qt em tres arquivos rotativos de
  1 MiB. Testes cobrem retencao, tamanho/quantidade dos arquivos e clique real
  no menu; screenshot offscreen confirmou contraste legivel.
- [RESOLVED] [SUPERVISOR-N3-ADMISSION-RACE] A transicao para
  `untrackedStopFailure` agora usa o mesmo `processRegistryMutex` da admissao de
  novos processos. Uma chamada concorrente nao pode mais observar estado
  parcialmente publicado. N3 e lifecycle passaram 5/5 com warnings fatais.
- [RESOLVED] [ROTATING-LOG-CONCURRENCY] `RotatingLogWriter` serializa
  `file_size`, rotacao e append por instancia. Linhas maiores recebem marcador
  de truncamento em vez de perda silenciosa. Os dois contratos, incluindo
  quatro writers concorrentes, passaram.
- [RESOLVED] [SAM-INVALID-TEMP-OWNER] Um `QTemporaryDir` invalido nao mantem
  mais `activeOutput_` envenenando a proxima tentativa. O fetch ainda relata a
  falha original de criacao; o descarte apenas libera um owner sem diretorio.
  Fetch multi-setor e retry apos falha real de cleanup passaram 4/4.

- [RESOLVED] [SQLITE-IMPORT-WRITER-LOCK-CAPABILITY] O construtor de
  `SqliteSsaImportWriter` exige uma capability privada concedida ao workflow
  que ja possui o lock canonico. Testes usam um acesso explicito isolado em
  `tests/`; nenhum lock interno foi adicionado, evitando auto-deadlock. Build
  do target de integracao e 8/8 contratos de writer, crash, journal, retomada e
  aliases de lock passaram.
- [RESOLVED] [COLUMN-CATALOG-PRESENTATION-LEAK] `domain::ColumnDef` contem
  somente chave, tipo e participacao na busca geral. Os 85 labels gerais estao
  no catalogo Qt-free de application; largura e visibilidade permanecem no
  catalogo de presentation. GUI e CLI passam cabecalhos CSV explicitamente no
  request do port; infra valida cardinalidade e usa chaves canonicas quando um
  adapter omite labels. O teste de cobertura exige correspondencia integral
  entre schema e labels, e 19/19 contratos focados passaram. A projecao default
  continua no dominio por ser politica compartilhada de consulta, nao visual.
- [RESOLVED] [STAGED-COPY-REPLACEMENT-RACE] Se a fonte desaparece na janela
  entre a copia e a verificacao final, depois de um snapshot inicial valido, o
  resultado agora e deterministicamente `source changed during staged file
  copy`. Falhas de permissao ou IO continuam como impossibilidade de verificar.
  O trio de staging passou e o reproducer de troca com mesmo tamanho/mtime
  passou 50/50 repeticoes.
- [RESOLVED] [DISTINCT-LIMIT-QML-COUPLING] O limite geral de consulta permanece
  no dominio. O limite de 5000 exclusivo do popup avancado agora pertence ao
  request builder de presentation; dominio nao menciona mais custo de QML.
- [REFUTED-LOCAL] [SEMGREP-WIDE-NARROW-TIMEOUT] A alegacao de duas regras
  locais narrow/wide introduzida em `63554b9` nao possui IDs, YAML, fixture ou
  log. Semgrep 1.170 focado no arquivo de 6,815 linhas passou sem timeout ou
  finding; nao dividir fixtures. A equivalencia exata ao pin 1.169 do CI ainda
  depende de Docker funcional e nao recebeu credito como validacao concluida.
- [RESOLVED-LOCAL] [CLANG-TIDY-COMPILE-DB] O compile database macOS agora recebe
  `CMAKE_OSX_SYSROOT=macosx` pelos cinco presets-raiz. `clang-tidy -p build/dev`
  encontra a standard library sem argumentos extras de sysroot; o gate focado
  passou. Plataformas nao Apple ignoram essa variavel CMake.
- [RESOLVED-HISTORICAL] [ROUND-STATUS-STALE] `ROUND_STATUS.md` foi reconciliado
  primeiro para o baseline intermediario `d34f92d`. Esse snapshot foi superado
  pelo HEAD publicado `d376431` e pelo marco P0 no working tree; o status atual
  distingue 443/443 no HEAD, o full 449/449 anterior ao benchmark e o smoke
  novo 1/1. A matriz configurada atual tem 450 testes, ainda sem full 450/450.
- [RESOLVED] [NINJA-CLEAN-CONCURRENCY] O erro `Directory not empty` ocorreu
  quando o smoke clean removeu `build/dev` durante uma compilacao Ninja. O
  script agora recusa a limpeza enquanto `.ninja_lock` existe e diagnostica
  uma corrida residual do `rm` sem continuar sobre build parcialmente apagado.
- [RESOLVED] [IMPORT-DEVIATION-NUMBER] Planilhas reais usam a coluna `Desvio`
  com inteiro puro ou rotulo `Desvio #<inteiro>`. O mapper converte somente
  esse formato explicito para `numero_desvios`; texto ambiguo continua
  fail-closed. A varredura read-only encontrou 14 valores distintos, todos
  cobertos, e os contratos mapper/workflow/SQLite passaram.
- [RESOLVED] [IMPORT-OPTIONAL-INVALID-DATE] O mapper apagava datas opcionais
  invalidas quando a normalizacao retornava vazio, impedindo `validateRow` de
  rejeitar a linha. Valores invalidos nao vazios agora sao preservados ate a
  validacao e contabilizados como `invalid_date`, sem expor conteudo da celula.
- [RESOLVED] [DESKTOP-SMOKE-OBJECT-NAMES] O harness source registra a factory
  do singleton C++ real `DesktopSmokeObjectNames` antes de carregar os dialogs.
  `ssa_qml_help_about_tests` passou offscreen sem os `ReferenceError` anteriores,
  preservando os `objectName` usados pelos contratos smoke.
- [RESOLVED] [QMLTYPES-MISSING]
  `build/dev/SsaConsultaRapida/ssa_consulta_rapida.qmltypes` existe e
  `cmake --build --preset dev --target all_qmllint` passou. O finding antigo
  estava obsoleto e nao exige patch.
- [RESOLVED-WT] [GRAPH-NODE-GEOMETRY-DUPLICATION] `118x48` possui fonte unica
  em `DerivadasGraphModel`; QML consome propriedades CONSTANT. Bounds, centros,
  arestas, hit-test e SVG permanecem cobertos.
- [PARTIAL-EXTERNAL] [GRAPH-EXPORT-URL-CONVERSION] O parsing manual `file://`
  saiu do QML. A fronteira C++ aceita somente `QUrl::isLocalFile/toLocalFile`,
  falha fechado para vazio/relativo/qrc/HTTP e cobre PNG Unicode no macOS.
  Windows/UNC ainda exige plataforma real.
- [RESOLVED] [GRAPH-TARGET-SELF-EDGE] O modelo ignorava a identidade do target
  ao classificar parents e children, permitindo papel incorreto e self-edge em
  entrada repetida. As duas passagens agora excluem o target e o teste focado
  prova node unico, papel `current`, status preservado e zero arestas.
- [RESOLVED] [DERIVED-SUMMARY-LOCK-BACKOFF] A falha `LockHeld` tentava adquirir
  novamente o lock a cada leitura, permitindo loop quente sob contencao. O mesmo
  backoff de um segundo das demais falhas transientes agora limita tentativas;
  o teste prova fallback readonly imediato e criacao do resumo apos o intervalo.

- [RESOLVED] [TSan] Data race em `QArrayDataPointer<char16_t>::deref()` corrigido. Causa raiz: `QFutureWatcher<T>` com T nao-trivial tem race no `ResultStore` durante teardown concorrente com o worker reportando resultado; alem disso `QtConcurrent::run` com cancel+setFuture imediato race o vtable do runnable. Correcoes aplicadas:
  - `UserPreferencesCoordinator`: migrado para `QFutureWatcher<void>` + erro em `std::string` protegido por mutex.
  - `FilterPanelDistinctValueFetcher`: migrado para `QFutureWatcher<void>` + resultado via `shared_ptr` guarded por mutex; requests concorrentes agora enfileirados (pending) em vez de cancel+setFuture imediato.
  - `MainPreferenceFlowCoordinator`, `PageQueryCoordinator` e `WorkflowCommandRunner`: migrados para `QFutureWatcher<void>` com estado compartilhado sincronizado; teardown sem `processEvents()`.
  Validado: dev-tsan 100/100 (2x consecutivas).

- [COVERAGE] Analise de gaps de cobertura (78.15% linhas agregado, meta 90%).
  Arquivos com cobertura < 70% (priorizar adicionar testes):
  - `query/SqlPredicateBuilder.cpp` (0%) - critico, compoe WHERE clauses
  - `query/SqlQueryText.cpp` (0%) - validacao/escape de identificadores SQL
  - `query/SqlQueryBuilder.cpp` (~6%) - gerador central de queries (testes unit existe mas nao cobre buildRows/buildCount)
  - `application/SsaBrowseService.cpp` (~1%) - servico de browse principal
  - `domain/ColumnValuePriorityPolicy.cpp` (~3%) - politica de prioridade de colunas
  - `infra/import/SsaSpreadsheetHeaderCatalog.cpp` (0%) - catalogo de headers de import
  - `infra/preferences/FilterPresetJsonCodec.cpp` (~1%) - codec JSON de presets
  - `ports/*.h` (0-1%) - interfaces, cobertura esperada baixa (headers)
  Gerar relatorio: `bash scripts/generate-coverage.sh` -> build/dev-cov/coverage_html/

- [STYLE] clang-tidy/cppcheck achados de estilo nao corrigidos (codigo pre-existente em SsaTypes.h/ColumnCatalog - refatoracao transversal fora de escopo):
  - `cppcoreguidelines-pro-type-member-init` em varias structs de dominio (AdvancedFilterSpec, SsaPageRequest, etc.)
  - `performance-enum-size` (enums usam int onde uint8 bastaria)
  - `portability-avoid-pragma-once` (pragma once e padrao moderno, mantido)
  - `bugprone-easily-swappable-parameters` (2 params int adjacentes em pageCount)
  - `functionStatic`/`constParameterReference`/`useStlAlgorithm` (varios - baixo impacto)
  Enderecar em ciclo dedicado de hardening de dominio, nao junto a mudancas funcionais.

- [STYLE] [CLAZY-BASELINE] Scan `level0,level1` encontrou 32 warnings unicos
  preexistentes. Principais grupos: 8 `range-loop-detach`, 5 includes diretos de
  modulos Qt, 9 tipos/getters moc, 7 `QString` globais estaticas, 2 `QString::arg`
  e 1 lambda sem contexto. O gate reviewdog filtra apenas linhas alteradas para
  impedir nova divida. Corrigir a baseline em slice C++ dedicado, com QtTest
  focado por grupo.

- [PARTIAL] [SEMGREP-CPP-PARSER] Semgrep CE 1.169.0 conclui o scan sem finding,
  mas emite 63 diagnosticos de parser no C++/Qt: 46 `PartialParsing`, 16
  `Other syntax error` e 1 `Syntax error`. Macros Qt, preprocessamento e C++
  moderno reduzem a cobertura semantica. Manter clang-tidy, clazy, cppcheck e
  CodeQL como compensacao. As seis regras QML locais possuem fixtures RED/GREEN
  e passam integralmente; criar regra QQmlSA apenas quando existir contrato
  semantico que a busca lexical nao represente com seguranca.

- [PENDING] [L2] Avaliar FTS5 para general search (`%contains%`). A busca default (MatchMode::Contains) produz `LIKE '%text%'` colapsado em N colunas, que nao e sargable mesmo com indices. FTS5 (virtual table + triggers) tornaria o general search indexado. Mudanca maior: exige schema versionavel, populate do indice no import e manutencao em updates. Adiar ate dataset real demonstrar lentidao.
- [PENDING] [L6] Bind de LIMIT/OFFSET como int64. Hoje bindings sao `std::vector<std::string>` (text) em todo o pipeline; SQLite coerciona em runtime sem custo real, mas o contrato deveria ser tipado. Exige variante de binding (text vs int64) atravessando SqlQuery/bindAll/fakes/testes. Ganho marginal; adiar ate outro refactor do query builder justificar o custo.
- [PENDING] [L7] `recordBySsaNumber` com `SELECT *`. A tela de detalhes mostra todas as colunas, entao `SELECT *` e apropriado hoje. Reavaliar se a tela de detalhes passar a projetar um subconjunto.

- [PENDING] [UX-NAV] Navegacao por setas <> no DetailsPanel e confusa. As setas percorrem a cadeia de derivadas da SSA em exibicao (Current/Mae/Filhas), mas ao carregar uma SSA relacionada a cadeia muda para a dela e o indice restaurado pode apontar para posicao inconsistente. Considerar historico de navegacao (back/forward) ou manter a cadeia original fixa durante a navegacao. Commit 990667c documenta o funcionamento atual em detalhe.

- [PENDING] [UX-TABLE] Usuario reporta linhas verticais "grossas" entre colunas de dados da tabela, mas analise de pixels no screenshot offscreen (3 medicoes) nao encontra linhas verticais estruturais - apenas pixels de texto. Pode ser problema de DPI/scaling/font rendering no monitor do usuario ou versao compilada intermediaria. Investigar com o usuario apontando exatamente onde ve as linhas em zoom.

- [PENDING] [UX-GRID-ALIGN] Apos o slice `e348c32` (merge de Macro e Reprogramacoes no grid), 3 refinos pendentes (reportados por usuario em validacao visual):
  1. **Macro card texto desalinhado**: o texto/label da Macro card esta "para cima" (topo), fora do alinhamento vertical dos demais titulos (Setor emissor, Prioridade emissao, etc). A Macro card usa `GridLayout` interno enquanto as text cards usam `ColumnLayout`. Padronizar para mesma baseline vertical.
  2. **Macro card texto deve ir ao mesmo lugar que os demais**: o titulo da Macro card deve estar na MESMA posicao relativa que os titulos das text cards (canto superior esquerdo, mesma fonte/tamanho). Hoje esta em posicao diferente.
  3. **Reprogramming card com espaco maior**: a Reprogramacoes card tem espaco interno maior que as outras (provavelmente por causa do `cardHeight: 70` vs `textCellHeight: 56`). Padronizar a altura para `56` igual as outras, ou revisar o layout interno para nao sobrar espaco.
  Cuidado: cada refino deve ser slice separado e cirurgico, sem quebrar o reflow do Flow + Repeater do `AdvancedTextFilterGrid.qml`.

## Consolidacao GUI QML/Qt (julho 2026) - pendentes

- [PENDING] [THEME-PY-AA] Revisar em slice proprio os 18 contrastes AA legados
  em `classicopy`, `darkpy`, `solarized-darkpy`, `solarized-lightpy`,
  `mint-lightpy` e `paperpy`. Este ciclo preserva as paletas Python importadas
  ipsis litteris e aplica o gate novo somente aos temas nativos adicionados.
- [PENDING] [GUI-CONTRACT-WEEK-DERIVATION] Decidir se a GUI deve restaurar o filtro generico de semana e o seletor de derivacao descritos em `docs/contracts/gui-behavior.md`. O runtime atual expoe apenas os cards de emissao e execucao e nao oferece controle visual para `derivationMode`. Nao alterar o layout ate uma decisao de produto explicita.
- [PENDING] [TYPESCALE-POINTSIZE] Migrar TypeScale de `font.pixelSize` para `font.pointSize` para respeitar a escala de fonte/DPI do SO (acessibilidade - fonte grande). Impacto: revalidar todos os 36 QML em Retina/HiDPI e telas com fonte do SO em Large. Slice dedicado.

## Cobertura de seguranca consolidada (julho 2026)

- Scanners removidos de Python, Node e DAST nao foram restaurados porque o repositorio nao possui superficie Python, Node ou servico HTTP implantado. A cobertura aplicavel permanece em `semgrep.yml`, `devskim.yml`, `defender-for-devops.yml`, `codeql.yml`, `dependency-review.yml` e `ci.yml`, com PR e agenda onde suportado.
- [EXTERNAL] Aguardar a restauracao da conta GitHub, hoje bloqueada por HTTP 403,
  antes de criar/configurar o environment GitHub `release` com revisores
  obrigatorios. O YAML referencia esse environment, mas essa dependencia nao
  bloqueia publicacao ou CI no GitLab e Bitbucket. Ver `ROUND_STATUS.md`.
