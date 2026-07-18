# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo
antes de interpretar sincronizacao Git, validacao local ou estado externo.

Ultima verificacao local: 2026-07-18

## Marco Activity, importacao parametrizada e SQLite

- Branch: `master`.
- HEAD de codigo validado: `360d18b`, precedido por `652b102`, `1cea684` e
  `2dd9aec`. Bitbucket foi confirmado em `360d18b`. O push GitLab `origin`
  falhou antes de transferir dados porque o OAuth continua com
  `invalid_grant`.
- O working tree tracked restante contem somente esta reconciliacao documental
  e mudancas locais preexistentes fora de escopo, listadas pelo `git status`.
- Plano original: `89.0/100 -> 99.0/100`. O P0 pertence a divida nova; 3.9
  pontos vieram dos menus, 0.8 dos popups, 1.6 do grafo e 3.7 do benchmark
  isolado de prefetch. Profiling valido permanece pendente por 1.0 ponto.
- Divida nova: `0.0/100 -> 78.6/100`, usando aceite binario dos 14 itens
  enumerados no handoff; 11 itens estao aceitos localmente. O novo credito e o
  benchmark concluido de `SQLITE-READ-CONNECTION-CHURN`; findings descobertos
  depois da enumeracao permanecem registrados fora desse denominador fixo.
- Backlog legado: `0.0/100 -> 0.0/100`; nenhum item legado recebeu credito.
- Estado separado: o full build, `581/581` e security ampla validam `4e74790`;
  o target de integracao e `7/7` focados validam `2dd9aec`; `3/3`, `45/45` e o
  benchmark de 30 processos validam `1cea684`. Em `360d18b`, o target de
  integracao, 60 repeticoes dos 2 contratos concorrentes, `13/13` da familia
  de derivadas e `8/8` de leitura/cancelamento passaram. Todos estao
  commitados localmente e `360d18b` foi comprovado no Bitbucket. GitLab e
  plataformas nao locais permanecem prova externa pendente.

### Fechamento local de 2026-07-18

- `91c60a1` endurece Activity Analytics: metadata, storage classes, data/semana,
  revisao ativa, pontos e disponibilidade falham fechado; reparo da semana
  corrente e deterministico e preserva configuracao valida.
- `eede38e` adiciona `ImportExecutionOptions`: chunks de 1 a 1,000 linhas e
  SQLite busy wait de 0 a 3,000 ms em passos de 5 ms, validados antes de staging
  e lock e propagados por full, incremental, external e recovery.
- `d54a693` separa primeira passagem de reabertura idempotente no benchmark,
  separa o gate RSS do benchmark Activity e mantem o probe POSIX fora de
  Windows.
- `4e74790` mede o repository SQLite real em 30 processos: first read,
  open/read/close repetido e 4 x 20 chamadas concorrentes de `page()`.
- `2dd9aec` substitui a troca de arquivo aberto do rescan por SQLite Backup API
  no handle de destino. Busy wait e stop token sao propagados; falha ou
  cancelamento antes de `SQLITE_DONE` preserva banco e fontes, enquanto commit
  concluido vence stop tardio e segue para consolidacao.
- `1cea684` especializa igualdade BINARY somente para `numero_ssa` canonico de
  9 digitos. Campos textuais, valores nao canonicos, busca geral, quick sector
  e status preservam `COLLATE NOCASE`; nenhum schema ou indice mudou.
- `360d18b` limita `derivedCountSummaryMutex_` a eleicao e publicacao de
  estado. Lock de arquivo, open, handlers, transacao, DDL/DML e log ficam fora
  do mutex; callers concorrentes usam fallback read-only e cancelamento ou
  excecao libera a eleicao para retry imediato.
- As oito falhas informadas do gate de 564 casos passaram novamente `8/8` em
  `10.09 s` no registro atual. A matriz configurada agora possui 587 casos.
  Activity/SQLite passou `79/79` em `3.23 s`.
- Em `4e74790`, build `dev` completo passou 476 passos e CTest sequencial
  passou `581/581` em `82.06 s`, incluindo RSS de importacao de 250 mil linhas
  em `13.67 s`.
- Primeira passagem, 30 amostras por cenario: canonical wall p50/p95
  `1009.864/1484.281 ms`; legacy com 250 mil updates
  `1959.458/3258.614 ms`. RSS adicional p95: `55,525,376/98,320,384` bytes.
- Reabertura idempotente, 30 amostras por cenario: canonical wall p50/p95
  `1.883/3.894 ms`; origem legacy ja canonizada `1.817/3.589 ms`; zero updates.
- Activity em 250 mil linhas: fingerprint `239.838 ms`, captura inicial
  `1871.504 ms`, repeticao idempotente `0.265 ms`; 6 snapshots e 6 pontos.
- Churn SQLite em 250 mil linhas: first read wall p50/p95
  `6.948/12.897 ms`; open/read/close repetido `1.390/3.296 ms`; RSS repeated
  p95 `16,384` bytes. As 2,400 chamadas concorrentes de `page()` tiveram
  latencia agregada p50/p95 `157.501/219.458 ms`, com cache de paginas do SO
  nao controlado. Pool ou handle persistente nao foi justificado.
- O `EXPLAIN QUERY PLAN` isolou a latencia de `page()`: `Equals` em
  `numero_ssa` compila com `COLLATE NOCASE`, incompatibilidade que transforma o
  indice BINARY em scan. No banco real, count/rows atuais visitaram 94,879
  linhas em `43/71 ms`; a comparacao BINARY produziu `SEARCH` em `2/<1 ms`.
  `1cea684` restringiu essa otimizacao ao numero SSA canonico de 9 digitos.
- Publicacao de rescan: os 3 casos antes RED passaram `3/3` em `0.22 s`; o gate
  ampliado de atomicidade, lock, cancelamento e consolidacao passou `7/7` em
  `0.35 s`. Review Sol encontrou 1 P1 de stop tardio apos `SQLITE_DONE`; o
  finding foi corrigido e o re-review terminou sem P0-P2.
- Igualdade SSA: os 2 contratos canonicos falharam RED enquanto o fallback
  NOCASE passou; depois do patch, os 3 casos passaram `3/3` em `0.12 s` e o
  gate correlato passou `45/45` em `2.02 s`. O planner usa `SEARCH` em page e
  count. Sol xhigh terminou sem P0-P3.
- Benchmark pos-fix, 30 processos e 250 mil linhas: 2,400/2,400 reads, zero
  falha/cancelamento. `page()` p50/p95 caiu de `157.501/219.458 ms` para
  `0.133/0.229 ms`; batch 4 x 20 caiu de `3281.940/14109.139 ms` para
  `2.902/4.241 ms`. Repeated open p95 ficou em `0.145 ms`.
- Mutex do resumo: o RED falhou em `fallbackReady` em `1.06 s`. Depois do
  patch, os 2 contratos passaram em `0.13 s`, 60 repeticoes passaram em
  `3.34 s`, a familia de derivadas passou `13/13` em `1.43 s` e o gate de
  transacoes/readAll/cancelamento/benchmark smoke passou `8/8` em `0.22 s`.
  Sol xhigh e Terra high encontraram a mesma lacuna P2 de cancelamento do
  initializer; o teste foi adicionado e os dois re-reviews terminaram em GO.
- Security ampla: Semgrep 507 arquivos e zero finding; Gitleaks 1.42 GB e zero
  leak; TruffleHog 8,853 chunks e zero segredo verificado ou desconhecido.
- Sol xhigh e Terra high deram GO final. Clawpatch nao gerou review porque seu
  Codex CLI interno e antigo para `gpt-5.6-sol`; isso nao recebeu credito.

Proxima atividade unica: provar a semantica WAL da publicacao do rescan. Um
leitor em transacao deve manter o snapshot antigo ate encerrar e observar o
novo snapshot na transacao seguinte.

## Marco P0 historico
- Ultimo slice: commit `e0b5401`, dois polls sincronos de `quickSector()`
  substituidos por `QCOMPARE`; o poll de `activeFilterEntries()` permaneceu
  por depender de timer.
- Slice seguinte: commit `41ecf36`, dois polls sincronicos de `textFilter()`
  removidos; os resumos de filtros continuam com polling por timer.
- Slice estrutural SAM: commit `14485a9`, leitura, metadados e adaptacao do
  workbook extraidos para uma operacao interna tipada.
- Slice estrutural XLSX: commit `627a075`, streaming, mapping, conflitos e
  escrita por chunk extraidos para `ChunkedWorkbookImportResult`; transacao,
  catches, rollback, journal e consolidacao permaneceram no orquestrador.
- Slice estrutural stager: commit `55c6bd8`, discovery/staging/cleanup ficaram
  em `ImportFileStager`, enquanto plan e moves ficaram em
  `ImportFileConsolidator`. O arquivo do stager caiu de 1364 para 634 linhas.

### Entregas locais do marco

- `Different` serializa canonicamente `!=valor`; leitura tipada de `!valor`
  legado permanece aceita. Busca geral `!valor` continua not-contains,
  `-valor` continua literal e `~` continua SafePattern.
- Distinct aplica filtros textuais avancados remanescentes. O request builder
  remove somente o self-filter da coluna alvo.
- `DesktopLogSink` reserva estado compartilhado sob o mutex global, executa
  formatacao, I/O, invoke Qt e handler anterior fora dele e drena handlers em
  voo antes de concluir o teardown.
- `RotatingLogWriter` trunca em fronteira UTF-8 valida, preservando cap, newline
  e sufixo.
- `.qmltypes` existe em `build/dev/SsaConsultaRapida/` e `all_qmllint` ja
  passou; o finding antigo esta obsoleto.
- A consolidacao ja usa handles de diretorio e rename no-replace ancorado.
  Falta prova externa Windows/UNC, nao implementacao local de handle.
- Importacao explicita de derivadas ja esta presente. Filtros genericos de
  semana e derivacao continuam ocultos para um ciclo GUI futuro.
- O benchmark de prefetch separa setup do processo, foreground wall/CPU, idle
  wall/CPU e RSS. O target de 30 amostras validou requests `[0,1,2]`, um count,
  um terminal inicial e cache das paginas 1 e 2. Nesta execucao, foreground
  wall ficou em `0.024146/0.050042 ms`, foreground CPU em
  `0.0275/0.049 ms`, idle wall em `0.028854/0.103333 ms`, idle CPU em
  `0.047/0.117 ms` e RSS em `16531456/16547840` bytes, mediana/p95.
- Validacao do ultimo slice: build `ssa_qt_filter_panel_tests` e CTest `1/1`
  passaram em `2.96 s`; clang-format, Semgrep, detect-secrets e hooks de
  segredo passaram. Nenhum credito adicional foi atribuido ao denominador
  binario de 14 itens.
- Validacao de `41ecf36`: build e CTest da mesma suite passaram `1/1` em
  `5.74 s`; nenhuma mudanca de producao.
- Validacao de `c95a757`: build e CTest passaram `1/1` em `2.88 s`; gates
  direcionados e hooks de segredo passaram.
- Performance: `roleNames()` foi medido por busca estrutural; cinco modelos,
  sem consumidor repetitivo de producao identificado. Cache nao foi adicionado.
- Corretude de importacao: caso adversarial com filename de 4096 digitos e
  timestamp valido passou `1/1` em `0.02 s`; sem credito de performance.
- Gate amplo anterior: CTest sequencial completo confirmado em duas etapas,
  total operacional `452/452`.
- Determinismo adicional: a tentativa `7ec88ae` de remover dois loops de
  polling foi rejeitada pelo gate amplo, pois `QTRY` bombeia eventos e quebra
  a janela preterminal. Commit corretivo `9b960ed` preserva os loops e o
  contrato; suite passou `1/1` em `0.75 s`, sem credito.
- Reteste pos-correcao: testes `431-448` passaram na etapa final, e `449-452`
  passaram `4/4` em `12.24 s`; o teste 434 passou novamente no estado corrigido.
- Gate do hub de importacao: CTest sequencial completo `453/453` passou em
  `64.05 s`. O novo teste de falha tardia preservou `conflicts=1` no resumo e
  confirmou rollback integral sem tabela publicada.
- Gate do stager hub: 11 contratos focados passaram `11/11` em `0.36 s`; o
  CTest sequencial completo passou `453/453` em `65.75 s`. O Sol xhigh e o
  Terra high aceitaram o split sem finding funcional. Windows/UNC real segue
  como prova externa pendente, portanto sem credito binario adicional.
- Security ampla do marco: Semgrep analisou 474 arquivos com zero finding;
  Gitleaks analisou 1.42 GB sem leak; TruffleHog analisou 8668 chunks sem
  segredo verificado ou desconhecido.

## Snapshot da v0.9.10 e reauditoria externa

- Branch: `master`.
- HEAD publicado antes desta reauditoria: `d376431` (`RELEASE: prepare v0.9.10
  handoff`).
- Tag anotada `v0.9.10` publicada em `origin` e `bitbucket`, dereferenciada para
  `d376431` nos dois remotes.
- `origin/master` e `bitbucket/master` foram confirmados em `d376431`.
- Working tree tracked estava limpo antes desta atualizacao documental.
- Sete entradas locais untracked permanecem fora de staging e da release.
- Nenhuma branch, worktree, PR, merge ou publicacao em GitHub foi criada.

## Estado implementado

### Importacao, atomicidade e banco

- O rescan publica banco e journal duravel antes de mover fontes.
- Crash, cancelamento e consolidacao parcial permanecem retomaveis.
- A retomada do journal ocorre antes da importacao do lote corrente.
- Falha antes da publicacao preserva banco e fontes originais.
- Staging compara identidade, tamanho e mtime e classifica desaparecimento
  depois do snapshot inicial como fonte alterada.
- `SqliteSsaImportWriter` exige capability do workflow que possui o lock.
- Locks usam identidade canonica do banco e cobrem aliases por symlink.
- Schema SQLite nao mudou e continua usando chaves ASCII canonicas.
- Headers PT/ES/EN sao normalizados por `SsaSpreadsheetHeaderCatalog`.
- `numero_desvios` aceita inteiro ou `Desvio #<inteiro>`; ambiguidades falham.

### Camadas C++ e exportacao

- `domain::ColumnDef` contem apenas chave, tipo e regra de busca geral.
- Os 85 labels gerais ficam em `application::SsaColumnLabelCatalog`.
- Largura e visibilidade ficam em presentation.
- GUI e CLI enviam cabecalhos CSV explicitamente pelo request do port.
- Infra valida cardinalidade e usa chaves canonicas quando labels sao omitidos.
- Presentation rejeita chave desconhecida em vez de criar fallback visual.

### Supervisor, GUI e grafo

- N3 permanece sticky e fail-closed quando o SO nao prova encerramento.
- `FailedToStop` nao destroi um `QProcess` leader ainda vivo.
- O supervisor reabre somente depois de drain comprovado.
- A GUI exibe 30 logs/erros recentes, selecionaveis e copiaveis.
- Logs Qt persistem em tres arquivos rotativos de 1 MiB.
- Cinco familias de menu possuem entrada por pointer real e efeito terminal no
  working tree; nenhum QML de producao precisou mudar.
- Popups foram reconciliados visualmente em 1180x940 e 1580x940, sem defeito
  reproduzido e sem mudanca de layout/QML.
- O grafo real cobre determinismo, bounds, overlap, centro, teclado e
  exportacao. O working tree usa geometria unica no model e URL local tipada;
  Windows/UNC permanece prova externa pendente.

### Performance medida

- Importacao de 250000 linhas passou com RSS adicional de 96387072 bytes no
  probe registrado.
- `qtd_derivadas`, 30 amostras: resumo 0.035/0.038 ms mediana/p95; fallback
  read-only com `GROUP BY` 10.168/10.574 ms.
- Cleanup de trigger por pai caiu de 10.143/10.701 ms para 0.255/0.309 ms.
- Prefetch, 30 execucoes: 73.572/74.577 ms e RSS maximo de 17661952 bytes,
  ainda incluindo startup/teardown QtTest.

## Validacao local autoritativa

- `cmake --preset dev`: passou.
- `cmake --build --preset dev`: passou.
- `cmake --build --preset dev --target all_qmllint`: passou.
- Reauditoria em `d376431`: `cmake --build --preset dev -j 6` passou sem
  trabalho pendente.
- Reauditoria em `d376431`: `ctest --preset dev --output-on-failure` passou
  443/443 em 72.09 segundos.
- Marco P0 no working tree: primeiro CTest integral encontrou 6 expectativas
  smoke obsoletas em 2 suites e terminou 447/449 em 75.49 segundos.
- Apos reconciliar somente 6 expectativas canonicas, o CTest integral
  sequencial passou 449/449 em 63.68 segundos.
- O smoke focado do benchmark passou 1/1 em 0.08 segundo. Com esse novo caso,
  a matriz configurada atual tem 450 testes; a suite integral 450/450 ainda
  nao foi executada.
- Build dev completo passou em 11.48 segundos. Build do app afetado passou em
  13.36 segundos.
- Semgrep amplo: 468 targets, zero finding. Gitleaks: 1.41 GB, zero leak.
  TruffleHog: 8413 chunks, zero segredo verificado ou desconhecido.
- `ssa_qml_help_about_tests`: 1/1 em 1.58 segundos no gate final dos cinco
  fluxos pointer real.
- Smokes de popup 1180/1580: 2/2 em 1.10 segundos; imagens inspecionadas em
  `build/dev/popup-smoke-{1180,1580}/`.
- Grafo: suites presentation/popup 2/2 em 10.95 segundos; `all_qmllint` passou
  em 0.68 segundo. PNG com espaco/Unicode e URLs invalidas foram cobertos.
- O caso de substituicao de fonte com mesmo tamanho/mtime passou 50/50.
- Suite presentation/filter e contratos CSV/CLI/staging passaram focados.
- Semgrep e detect-secrets: zero finding bloqueante.
- Gitleaks e TruffleHog: zero segredo encontrado.
- CodeRabbit do fechamento funcional: 1 major e 1 minor, ambos corrigidos;
  gates repetidos.
- cppcheck manteve um falso positivo conhecido para referencia inicializada por
  `state_(state)` em `ColumnFilterViewModel`.

Os testes `ssa_gui_version` e `ssa_cli_version` passaram com `0.9.10`. A suite
443/443 desta reauditoria prova o codigo commitado em `d376431`. A suite
integral 449/449 prova o marco P0 anterior ao benchmark; o novo smoke passou
1/1 separadamente e elevou a matriz configurada para 450 testes. Nao existe
ainda prova integral 450/450, de commit, remote ou outra plataforma.

## Reauditoria Cursor GLM e OpenCode

Os dois relatorios recebidos usaram `d34f92d`. Cada finding foi reconferido no
HEAD `d376431`; nenhuma sugestao foi aceita apenas pela severidade alegada.

### Confirmados com impacto funcional ou de concorrencia

| ID consolidado | Evidencia no HEAD | Decisao |
| --- | --- | --- |
| `FILTER-DIFFERENT-SEMANTICS` | O HEAD possui a divergencia; o working tree serializa `!=valor` e preserva leitura legada e busca geral. | Resolvido e validado localmente no P0; falta commit/prova externa. |
| `DISTINCT-ADVANCED-TEXT-FILTERS` | O HEAD omite filtros avancados; o working tree aplica os filtros remanescentes e preserva a remocao do self-filter na fronteira presentation. | Resolvido e validado localmente no P0; falta commit/prova externa. |
| `DESKTOP-LOG-HANDLER-IO-LOCK` | O HEAD faz I/O sob mutex; o working tree usa estado compartilhado, reserva in-flight e drain fora do mutex global. | Resolvido e validado localmente no P0; resta uma janela Minor de perda de log no exato uninstall. |

### Confirmados como risco estrutural ou divida controlada

- `IMPORT-WORKFLOW-HUB` foi fechado nos commits `14485a9` e `627a075`: leitura
  e adaptacao SAM, mais streaming/mapping/escrita XLSX por chunks, agora usam
  resultados internos tipados. Sessao SQLite, catches, rollback, journal e
  consolidacao continuam no orquestrador para preservar atomicidade.
- `IMPORT-STAGER-HUB` foi separado localmente em `55c6bd8`: staging pode mutar
  somente copias owned pre-commit; apenas o consolidator move o corpus fonte
  post-commit. As validacoes de path ficam compartilhadas sem duplicacao.
- `ImportFileStager.cpp` possui 1364 linhas. Antes de qualquer split, separar a
  matriz de discovery/staging da consolidacao por um desenho com invariantes de
  path, identidade e cancelamento.
- `SsaColumnDisplayCatalog` associa as 85 larguras por key canonica, com valores
  preservados e teste de cardinalidade/sentinelas. O finding posicional esta
  resolvido no working tree.
- `SqliteSsaRepository` abre conexao por operacao. Isto preserva isolamento por
  thread e cancelamento; tratar como candidato de benchmark, nao como bug.
- `derivadasDiretas()` usa `SqlQueryBuilder::buildDirectDerivations()` com
  binding tipado; o SQL e a ordenacao foram preservados.
- `BrowseRequestCoordinator` usa diretamente `filters_.excludeScaSesSte()`;
  o cast concreto e a dereferencia sem check foram removidos no working tree.
- `completenessScore()` usa diretamente o `value` ja iterado, removendo lookup
  e alocacao redundantes sem alterar o score.
- O parser de timestamp de filename tenta parse a partir de cada digito. O
  custo quadratico e limitado pelo tamanho de filename; medir antes de ampliar
  o parser.
- `normalizeExistingSsaNumbers()` percorre toda a tabela na abertura da sessao
  de import incremental. E risco real de escala, mas requer benchmark e desenho
  de migracao/normalizacao, nao uma troca local especulativa.

### Refutados, intencionais ou obsoletos

- A6-1: nao existe `defaultWidth == 98` em `DomainTests.cpp`; o teste citado
  pertence ao round-trip de preferences e nao e pixel magic de dominio.
- B22-4: 1769 arquivos e corpus explicitamente aceito pelo stager; nao e o
  limite de selecao externa.
- C1 e C3: os trechos sao cadeias flat de dispatch, nao condicionais aninhados
  de profundidade 7/13. Podem ser melhorados por legibilidade, mas nao ha custo
  algoritmico relevante que justifique tabela ou wrapper agora.
- D6: presentation depender do facade `application::SsaWorkflowService` respeita
  a direcao de camadas atual; trocar por outra interface sem necessidade seria
  duplicacao de port.
- N6: o C++ preserva `-valor` como literal por contrato testado. Paridade com a
  negacao Python exige decisao de produto, nao correcao automatica.
- N8: `~` usa SafePattern restrito por contrato de seguranca; nao sera convertido
  em regex Python sem decisao explicita.
- A afirmacao de watchers com resultado Qt nao-trivial esta obsoleta: os fluxos
  principais migraram para `QFutureWatcher<void>` e estado sincronizado. Ainda
  existem oportunidades separadas de teardown/espera, sem reabrir o finding
  TSan ja resolvido.
- `RotatingLogWriter::rename` usa overload que lanca em falha; nao ignora erro.
  O truncamento UTF-8 e sua cobertura foram corrigidos no working tree. O custo
  de abrir o arquivo por linha permanece candidato de medicao, fora do P0.

## Pendencias ativas priorizadas

1. Retirar lock e I/O SQLite de dentro de `derivedCountSummaryMutex_`, mantendo
   um unico inicializador e cancelamento por caller.
2. Produzir profiling valido do prefetch; `xctrace` nao oferece `Time Profiler`
   nesta instalacao e `CPU Counters` falhou com `DTServiceHub`/politica do
   kernel.
3. Revalidar a publicacao do rescan com reader WAL em transacao.
4. Canonizar `clang-tidy` macOS com sysroot e reproduzir IDs Semgrep antes de
   dividir fixtures.
5. Revalidar handles, PowerShell, packaging e URL UNC em plataformas reais.

Detalhes, criterios e itens de prioridade menor ficam em
`RECOVERY_BACKLOG.md`. O plano executavel fica em
`docs/plans/2026-07-17-v0.9.10-audit-handoff.md`. O plano anterior
`docs/plans/2026-07-17-v0.9.10-follow-up.md` permanece como historico da
preparacao da release.

## Remotes e publicacao

| Remote | Provedor | Funcao | Estado confirmado |
| --- | --- | --- | --- |
| `origin` | GitLab | Repositorio e CI primarios | push de `360d18b` bloqueado por OAuth `invalid_grant` |
| `bitbucket` | Bitbucket | Mirror obrigatorio | `360d18b` confirmado em `master`; tag `v0.9.10` preservada |
| `gh` | GitHub | Mirror inativo | HTTP 403; conta suspensa |

Todo fetch ou pull operacional vem de `origin`:

```bash
git fetch origin master
git pull --ff-only origin master
```

Nunca usar Bitbucket ou GitHub como fonte de pull. Um pedido de commit inclui
push para `origin` e `bitbucket`, seguido de `git ls-remote` nos dois. Tags
novas tambem devem ser publicadas e verificadas nos dois remotes.

## Estado externo

- GitLab CI pode falhar instantaneamente por `ci_quota_exceeded`; isso nao e
  diagnostico de codigo.
- Bitbucket Pipelines permanece sujeito a cota mensal compartilhada.
- GitHub Actions e environment `release` continuam indisponiveis enquanto a
  conta responder HTTP 403.
- Windows, Linux e pacotes de distribuicao nao foram revalidados nesta
  preparacao documental.

Este snapshot distingue implementacao local, commit, validacao local e prova
externa. Execute novamente os comandos ao vivo antes de afirmar estado remoto.
