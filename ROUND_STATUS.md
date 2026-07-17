# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo
antes de interpretar sincronizacao Git, validacao local ou estado externo.

Ultima verificacao local: 2026-07-17

## Marco P0 no working tree

- Branch: `master`.
- HEAD operacional contem os slices pequenos desta rodada, commitados e
  publicados em `origin` e `bitbucket`; o hash atual deve ser lido pelos refs
  remotos no fechamento da rodada.
- O working tree implementa localmente os slices de filtros/distinct e
  logging/UTF-8, alem das expectativas smoke canonicas correspondentes.
- Plano original: `89.0/100 -> 99.0/100`. O P0 pertence a divida nova; 3.9
  pontos vieram dos menus, 0.8 dos popups, 1.6 do grafo e 3.7 do benchmark
  isolado de prefetch. Profiling valido permanece pendente por 1.0 ponto.
- Divida nova: `0.0/100 -> 71.4/100`, usando aceite binario dos 14 itens
  enumerados no handoff; 10 itens estao aceitos localmente. O polling
  deterministico foi validado como melhoria de teste, sem credito adicional
  fora desse denominador.
- Backlog legado: `0.0/100 -> 0.0/100`; nenhum item legado recebeu credito.
- Estado separado: implementado e commitado no HEAD; validado localmente nesta
  rodada; prova externa limitada aos refs GitLab e Bitbucket.
- Ultimo slice: commit `e0b5401`, dois polls sincronos de `quickSector()`
  substituidos por `QCOMPARE`; o poll de `activeFilterEntries()` permaneceu
  por depender de timer.
- Slice seguinte: commit `41ecf36`, dois polls sincronicos de `textFilter()`
  removidos; os resumos de filtros continuam com polling por timer.

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

- `SpreadsheetImportWorkflowPort.cpp` possui 1285 linhas e 23 catches; o metodo
  `importDiscoveredFiles()` ocupa 454 linhas e concentra 11 catches, mapping em
  chunks, sessao SQLite, resumo e journal/consolidacao. A decomposicao precisa
  seguir responsabilidades ja provadas, sem criar ports ou wrappers genericos.
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

1. Produzir profiling valido do prefetch; `xctrace` nao oferece `Time Profiler`
   nesta instalacao e `CPU Counters` falhou com `DTServiceHub`/politica do
   kernel.
2. Executar as demais fronteiras pequenas aprovadas.
3. Desenhar e executar a decomposicao controlada dos hubs de importacao.
4. Medir normalizacao, conexoes, filename e `roleNames()` antes de otimizar.
5. Canonizar `clang-tidy` macOS com sysroot e reproduzir IDs Semgrep antes de
   dividir fixtures.
6. Revalidar handles, PowerShell, packaging e URL UNC em plataformas reais.

Detalhes, criterios e itens de prioridade menor ficam em
`RECOVERY_BACKLOG.md`. O plano executavel fica em
`docs/plans/2026-07-17-v0.9.10-audit-handoff.md`. O plano anterior
`docs/plans/2026-07-17-v0.9.10-follow-up.md` permanece como historico da
preparacao da release.

## Remotes e publicacao

| Remote | Provedor | Funcao | Estado confirmado |
| --- | --- | --- | --- |
| `origin` | GitLab | Repositorio e CI primarios | `d376431` e `v0.9.10` confirmados |
| `bitbucket` | Bitbucket | Mirror obrigatorio | `d376431` e `v0.9.10` confirmados |
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
