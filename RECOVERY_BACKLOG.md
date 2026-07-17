# Recovery Backlog

## Pendente (fora do escopo desta trilha)

### Sequencia ativa preservada

1. Completar profiling valido do prefetch; harness, 30 amostras e relatorio
   isolado ja estao resolvidos no working tree. `xctrace` nao possui `Time
   Profiler` nesta instalacao e `CPU Counters` falhou com `DTServiceHub` e
   politica do kernel; nenhum credito de profiling foi atribuido.
2. Apresentar desenho de decomposicao dos dois hubs de importacao e executar
   somente fronteiras com responsabilidade comprovada.
3. Canonizar `clang-tidy` macOS com sysroot e reproduzir IDs das regras Semgrep
   antes de dividir fixtures.
4. Revalidar em plataformas reais handles, URL UNC, PowerShell e packaging.

Contadores apos os slices pequenos: plano original `99.0/100`, divida nova
`50.0/100` (7 de 14 itens aceitos) e backlog legado `0.0/100`.

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

- [HIGH-DESIGN] [IMPORT-WORKFLOW-HUB] `SpreadsheetImportWorkflowPort.cpp` tem
  1285 linhas e 23 catches. `importDiscoveredFiles()` concentra 454 linhas e 11
  catches para mapping, chunks, sessao SQLite, resumo, journal e consolidacao.
  Nao criar quatro interfaces por contagem de linhas. Primeiro extrair uma
  operacao interna de importacao de workbook/chunks com resultado tipado; depois
  avaliar uma operacao de publicacao/consolidacao que preserve journal-before-move.
  Aceite: mesmas mensagens/status/resumos, nenhum movimento antes do journal,
  cancelamento pre-commit faz rollback e interrupcao pos-commit fica retomavel.
  Arquivos candidatos: workflow, writer/stager existentes e testes de integracao.

- [HIGH-DESIGN] [IMPORT-STAGER-HUB] `ImportFileStager.cpp` tem 1364 linhas e
  mistura discovery, staging, inventory, plano e consolidacao. Mapear funcoes e
  dependencias antes do split. Uma fronteira aceitavel separa discovery/staging
  read-only da consolidacao mutavel sem duplicar validacao de path/identidade.
  Aceite: contratos atuais de symlink, alias, source identity, cancelamento e
  retomada continuam verdes em POSIX e Windows.

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

- [MED-MEASURE] [SQLITE-IMPORT-NORMALIZE-FULL-SCAN]
  `normalizeExistingSsaNumbers()` percorre todos os registros e referencias na
  abertura da sessao incremental. Medir 250 mil linhas com banco ja normalizado,
  CPU/RSS e cancelamento antes de escolher flag de schema, migracao ou
  normalizacao incremental. Nao pular validacao de colisoes semanticamente.

- [MED-MEASURE] [SQLITE-READ-CONNECTION-CHURN] O repository abre conexao,
  busy/progress handlers e statements por read. Isso tambem isola threads e
  tokens de cancelamento, portanto nao e bug comprovado. Medir cold/warm,
  keystrokes concorrentes e RSS antes de propor conexao longeva ou pool.

- [LOW] [SQL-DERIVADAS-BUILDER-BOUNDARY] `derivadasDiretas()` monta a consulta
  no repository em vez de `SqlQueryBuilder`. Os nomes atuais sao literais ou
  table name validado, portanto nao ha injection demonstrada. Mover a query para
  a camada query em slice pequeno, com bindings e teste unitario.

- [RESOLVED-WT] [BROWSE-SECTOR-CAST-INVARIANT] `BrowseRequestCoordinator` agora
  usa a API publica `FilterPanelViewModel::excludeScaSesSte()` diretamente. O
  include concreto, o cast e a dereferencia sem check foram removidos; o teste
  focado de exclusao passou 1/1.

- [RESOLVED-WT] [IMPORT-COMPLETENESS-LOOKUP] `completenessScore()` usa o
  `value` ja iterado em vez de repetir `map::find` e alocacao de key. Os casos
  de richness e metadata passaram 2/2.

- [LOW-MEASURE] [IMPORT-FILENAME-TIMESTAMP-SCAN] O parser tenta todos os sufixos
  iniciados por digito. O pior caso e quadratico, mas filename e limitado pelo
  filesystem. Adicionar benchmark adversarial antes de substituir o parser.

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

- [LOW] [QT-ROLENAMES-CACHE] Medir e, se houver ganho comprovado, armazenar
  `roleNames()` estavel por modelo para evitar reconstrucoes pequenas. Nao
  alterar o contrato Qt nem adicionar cache sem evidencia de hot path.

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
- [MED] [P2] status-last CASE sort (`UPPER(COALESCE(...))<>'STE'` por linha) nao-sargable, forca full-sort. Generated column `_status_is_ste` + composite index resolveria, mas exige migration robusta para DBs antigos (CREATE TABLE IF NOT EXISTS nao recria). Revertido - precisa de ALTER TABLE idempotente + fallback no builder.
- [MED] [P4] Macro report agrupa em memoria (`map<ReportKey,set<string>>` so pra .size()). Deveria ser `GROUP BY ... COUNT(DISTINCT)` em SQL.
- [MED] [P7] Eager date/string formatting: formata 500x12 celulas antes de exibir; QML so renderiza visiveis. Lazy exigiria repensar `SsaTableDisplayValues`/`displayCache_` - ciclo dedicado.

### Qualidade / duplicacao
- [RESOLVED] [Q1] As tres copias identicas de `trimCopy` da importacao foram
  consolidadas em `domain/WhitespaceTrim.h`, com prova Qt-free para texto,
  vazio e somente whitespace. Helpers de parser e `string_view` com semanticas
  diferentes permaneceram separados.
- [RESOLVED] [Q4] `SsaImportConflictResolver` usa somente `indexBySsa` para
  localizar linhas aceitas; o antigo `seenNumbers` redundante nao existe no
  HEAD atual.

### Cleanup / low priority
- [LOW] [L-Q2] `emptyFiles`/`failedFiles` contadores mortos no import (failedFiles so pode ser 0 ou 1).
- [LOW] [L-Q3] null checks redundantes em chaves vindas do proprio catalogo (AdvancedTextFilterRowModelFactory, ColumnFilterViewModel).
- [RESOLVED] [L-Q4] `tokenOperatorForStorage` foi removida; o enum tipado e
  armazenado diretamente.
- [PRODUCT] [L-A1] `SearchParser` preserva `-valor` como texto literal por
  contrato testado. Python usa hifen como negacao. Decidir paridade antes de
  alterar; nao quebrar busca de conteudo com hifen por inferencia.
- [RESOLVED] [L-A2] A semana 53 agora e aceita somente em anos ISO longos,
  com cobertura para 2020 (valido) e 2021 (invalido).
- [LOW] [L-A3] `LegacySpreadsheetConverter:117` `error` nao limpo apos `remove`.
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
- [TOOLING] [SEMGREP-WIDE-NARROW-TIMEOUT] As duas regras locais de conversao
  narrow/wide excedem o limite ao analisar o arquivo gigante
  `SpreadsheetImportWorkflowPortTests.cpp`. Os demais scans concluem sem
  finding. Separar fixtures ou criar harness focado antes de tratar essas duas
  regras como gate confiavel.
- [TOOLING] [CLANG-TIDY-COMPILE-DB] `clang-tidy -p build/dev` nao encontra
  headers da standard library/toolchain, incluindo `type_traits`, apesar do
  compile database existente. Corrigir a geracao ou a invocacao do toolchain
  antes de considerar clang-tidy um gate real; falha da ferramenta nao equivale
  a aprovacao do codigo.
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
