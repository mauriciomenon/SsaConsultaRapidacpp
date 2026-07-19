# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo
antes de interpretar sincronizacao Git, validacao local ou estado externo.

## Parametros de execucao de importacao ligados ao workflow - 2026-07-19

- **ENTREGUE localmente**: `WorkflowCommandViewModel` aplica e persiste os
  parametros `rows_per_chunk` e `sqlite_busy_wait_ms` do schema 14, com
  limites e granularidade validados antes de criar o request.
- **ENTREGUE localmente**: `WorkflowCommandRunner` propaga os parametros para
  importacao externa e reescaneamento. O SQLite continua usando as opcoes
  tipadas existentes; nao houve mudanca de schema de dados.
- **ENTREGUE localmente**: Preferencias agora exibem os dois parametros com
  controles limitados a 1..1000 linhas e 0..3000 ms em passos de 5 ms.
- Review final do diff: diff check, clang-format, cppcheck, Semgrep
  C/security, detect-secrets e qmlformat sem findings.
- Validacao local: build de `ssa_qt_presentation_tests` e `all_qmllint`; CTest
  focado `ssa_qt_presentation_tests` + `ssa_qml_layout_smoke_1180x760`, 2/2 em
  `10.46 s`, e suite completa `619/619` em `72.05 s`. O teste novo prova os
  valores em importacao e rescan.
- Contadores: plano original `99.0/100`; divida nova P0 `14/14 = 100.0%`;
  backlog legado `N/A`; fila operacional `6/8 = 75.0%` antes da prova
  Windows/UNC.

Estado apos commit local: `0c28720`; publicacao nos remotes sera confirmada
abaixo, mantendo a prova Windows/UNC como dependencia externa.

## Importacao externa em blocos e normalizacao de desvios - 2026-07-19

- **ENTREGUE e commitado**: selecoes externas com mais de 64 arquivos nao
  sao mais rejeitadas. O C++ divide o trabalho em blocos sequenciais de ate
  64, valida cada bloco, cria staging atomico e executa a transacao SQLite e o
  journal do bloco antes de seguir para o proximo.
- Prova ponta a ponta: 65 XLSX externos em 2 blocos, 65 insercoes no SQLite,
  65 arquivos em `processadas`, sem falha, teste focal `1/1` em `0.40 s`.
  O stager isolado tambem passou `1/1` em `0.34 s`.
- **ENTREGUE e commitado**: `numero_desvios` aceita inteiro puro,
  `Desvio #N` com espacos, `sem desvio`/`sem desvios` e descarta texto nao
  numerico opcional sem abortar a planilha. O teste inclui os tres casos e
  passou junto com o fluxo externo.
- **ENTREGUE e commitado**: o slice de preferencias schema 14 foi fechado.
  Valores `import_execution` ausentes ou de tipo invalido mantem os defaults;
  migracao schema 12 e persistencia schema 14 passaram.
- Review final do diff: `git diff --check`, clang-format, cppcheck,
  Semgrep C/security (2 regras, 0 findings) e detect-secrets passaram.
  Achados corrigidos durante o ciclo: formatacao, parametro por valor,
  assinatura/constante do lote e default de busy wait ausente.
- Suite completa sequencial: `619/619` em `70.29 s`. Nao houve timeout
  contabilizado como sucesso.
- Estado apos publicacao: branch `master`, HEAD `8955a6f`; `origin/master` e
  `bitbucket/master` confirmados no mesmo hash. Working tree continua dirty
  somente por arquivos locais preexistentes fora do slice; nenhuma prova
  externa foi presumida.
- Contadores: plano original `99.0/100`; divida nova P0 permanece
  `14/14 = 100.0%`, com esta regressao operacional fechada fora do denominador
  historico; backlog legado `N/A`; fila operacional local `6/8 = 75.0%`, com
  Windows/UNC real ainda sem credito.

Proxima atividade unica: iniciar a proxima pendencia local de alto impacto,
mantendo Windows/UNC como prova externa separada.

## Corpus atual no binario recem-compilado - 2026-07-19

- **ENTREGUE**: a copia descartavel do corpus atual foi processada pelo CLI
  recem-compilado, sem tocar `docs_entrada` do repositorio.
- Resultado real: `files=1692`, `rows=458864`, `inserted=96479`,
  `updated=362385`, `unchanged=1241540`, `skipped=256115`,
  `duplicates=1603953`, `conflicts=0`, `invalid_rows=0`,
  `invalid_number=0`, `invalid_description=0`, `invalid_date=0`,
  `failed=0`, `legacy_xls=135`, `unsupported=2`.
- SQLite descartavel passou `PRAGMA integrity_check=ok` e terminou com
  `COUNT(*)=96479`. Esta e a prova direta do fluxo completo apos os dois
  slices de importacao; nao depende do clone anterior.
- Estado Git: branch `master`, HEAD `4052c2a`; `origin/master` e
  `bitbucket/master` confirmados no mesmo commit. Working tree continua dirty
  somente por arquivos locais preexistentes nao relacionados ao slice.
- Contadores: plano original `99.0/100`; divida nova `14/14 = 100.0%` no P0;
  backlog legado `N/A`; fila operacional `5/8 = 62.5%` por provas externas.

Proxima atividade unica: selecionar outra pendencia local de alto impacto ou
executar a prova Windows/UNC real.

## Diagnostico por arquivo no importador - 2026-07-19

- **ENTREGUE**: `ImportFileResult` agora preserva por arquivo os contadores de
  `invalid_number`, `invalid_description` e `invalid_date`. A rejeicao de full
  rescan tambem informa `file=<nome>` e os tres contadores na mensagem publica.
- RED/GREEN: o caso de linha invalida passou a provar a origem
  `file=mixed.xlsx invalid_number=1`; os tres casos focados passaram `3/3` em
  `0.09 s`. Nao ha conteudo de celula sensivel na mensagem.
- Review inicial encontrou somente quebra de clang-format; a correcao foi
  reaplicada e o review final de diff, clang-format, cppcheck, Semgrep C e
  detect-secrets terminou sem findings.
- CTest sequencial final passou `618/618` em `72.23 s`, incluindo importacao,
  SQLite, journal, filtros avancados, QML e benchmarks smoke.
- Nenhum schema, layout, menu ou regra de aceite foi alterado. Contadores:
  plano original `99.0/100`; divida nova `14/14 = 100.0%` no P0; legado
  `N/A`; fila operacional `5/8 = 62.5%` por provas externas.

Proxima atividade unica: selecionar outra pendencia local de alto impacto ou
executar a prova Windows/UNC real.

## Regressao de `Prazo Limite` fechada - 2026-07-19

- **ENTREGUE**: foi acrescentado um teste de workflow que semeia uma SSA ja
  existente, importa uma linha com `Prazo Limite= Dentro do Prazo` e
  `Data Limite=2026-07-31`, e verifica a atualizacao no mesmo SQLite.
- O teste prova que `Prazo Limite` vai para `status_execucao_prazo` textual,
  `Data Limite` vai para `data_limite`, o valor legado `prazo_limite` nao e
  sobrescrito e a importacao termina com `invalid_rows=0` e
  `invalid_number=0`. Uma planilha real com esse cabecalho tambem foi
  executada no binario atual: `38/38` linhas aceitas.
- RED inicial foi somente uma expectativa errada do teste (`data_limite` e
  armazenada como `YYYY-MM-DD`); o ajuste foi revisado e o GREEN passou `1/1`
  em `0.04 s`.
- CTest sequencial final passou `618/618` em `70.92 s`. Isto inclui a familia
  completa de importacao/SQLite e os testes de filtros avancados.
- Contadores permanecem: plano original `99.0/100`; divida nova `14/14 =
  100.0%` para o P0 de importacao; backlog legado `N/A`; fila operacional
  `5/8 = 62.5%` por provas externas ainda sem credito.
- O erro `invalid_number=994` do print nao foi reproduzido no binario atual;
  a evidencia anterior de corpus integral permanece `invalid_number=0`. Nao
  ha alteracao de schema nem de regra de importacao nesta rodada, somente a
  prova de regressao.

Proxima atividade unica: continuar uma pendencia local de alto impacto ou
executar a prova Windows/UNC real; nao reabrir o mapeamento ja validado.

## Importacao integral do corpus corrigida - 2026-07-19

- **ENTREGUE**: o importador C++ concluiu o rescan isolado de 1692 XLSX. O
  resultado foi `rows=458864`, `invalid_rows=0`, `invalid_number=0`,
  `invalid_description=0`, `invalid_date=0`, `failed=0`.
- Foram corrigidos quatro formatos reais sem abrir o contrato geral: `Prazo
  Limite` e status textual, rotulos `Reschedule #N`/`Reprogramacao #N`,
  relatorios auxiliares de derivadas/relacoes e export bruto SAM. Linhas
  historicas incompletas de `Todas as SSAs` sao ignoradas apenas nesse perfil;
  relatorios normais continuam fail-closed.
- Evidencia SQLite no clone `/private/tmp/ssa_import_validation_19/ssas.db`:
  `PRAGMA integrity_check` retornou `ok`, `COUNT(*)=96479`, sem schema novo.
  O mesmo clone registrou `inserted=96479`, `updated=362385`,
  `unchanged=1241540`, `duplicates=1603953`, `conflicts=0`.
- Testes desta rodada: 5/5 casos focados de mapeamento, build dos targets
  `ssa_integration_tests` e `ssa_consulta_rapida_cli`, e rescan integral com
  aproximadamente 12 minutos observados. Review imediato: diff check,
  clang-format, Semgrep C/security e detect-secrets, todos sem findings.
- Contadores atuais: plano original `99.0/100`; divida nova `14/14 = 100.0%`
  para o P0 de importacao reproduzido; backlog legado `N/A` sem denominador;
  fila operacional `5/8 = 62.5%`, pois Windows/UNC real e outras provas
  externas continuam sem credito local.
- Risco residual: o corpus foi validado em clone macOS; nao e prova de
  Windows/UNC, SAM dedicado real ou todos os arquivos futuros. Nenhuma fonte
  de producao foi movida nesta validacao.
- Fechamento: CTest final passou `617/617` em `67.61 s` apos o ajuste de
  expectativa do teste incremental. O codigo esta em `5e3de3b`; o fechamento
  documental esta publicado no Bitbucket; `bitbucket/master` contem este
  fechamento. `origin/master` permanece em `428302d` por OAuth GitLab
  `invalid_grant`, sem alegacao de sincronizacao.

Proxima atividade unica: iniciar a prova Windows/UNC real ou selecionar outro
item externo, sem reabrir este P0 ja aceito.

## Reconciliacao ativa - 2026-07-19

- HEAD publicado: `874e31b` em `master`, `origin/master` e
  `bitbucket/master`. O slice corrige apenas a regressao visual dos botoes:
  `ActionButton` usa pixel size novamente e os botoes `Importar XLSX` e
  `Preferencias` usam `Theme.fontSizeLabel`.
- Validacao desta rodada: RED de fonte falhou como esperado; GREEN
  `ssa_qml_theme_gallery_tests` `1/1` em `0.70 s`; smoke da janela principal
  `ssa_qml_layout_smoke_1180x760` `1/1` em `3.95 s`; `all_qmllint` e hook
  staged passaram. Nenhum menu ou fluxo de banco foi alterado.
- Fila ativa: [IMPORT-WINDOWS-UNC-EXTERNAL] e provas de plataforma. O antigo
  [IMPORT-CORPUS-INVALID-NUMBER] foi resolvido e mantem a evidencia acima.
- Itens de filtros e distinct foram retirados da fila ativa: `!STE` significa
  not-contains e `!=STE` significa diferente exato; distinct remove somente o
  self-filter e aplica os demais filtros avancados. O legado fica `N/A`, sem
  denominador ativo inventado. Plano original `99.0/100`; divida nova
  `13/14 = 92.9%`.

Ultima verificacao local: 2026-07-18

## Gate de estabilizacao para v0.9.11 - 2026-07-18

- CMake `dev` reconfigurou e build canonico passou. O unico warning foi a
  deprecacao preexistente no source cache de miniz, fora do diff e sem mudanca
  de comportamento.
- CTest sequencial completo passou `602/602` em `64.93 s`, incluindo os
  checks de versao GUI/CLI `0.9.11`, importacao, SQLite, derivadas, QML,
  memoria e benchmarks smoke. Nenhum timeout foi aceito como sucesso.
- Release security: Semgrep completo passou `24` regras em `512` arquivos;
  Gitleaks escaneou `1.43 GB` sem segredo; TruffleHog terminou sem segredo
  verificado ou desconhecido. Os scanners staged dos dois commits tambem
  passaram.
- A tag anotada `v0.9.11` marca o gate de release `84462d4`. Bitbucket
  confirmou `v0.9.11^{}` em `84462d4` e `master` em `6a0bfa4`, que registra a
  publicacao do gate. GitLab `origin` recusou push por OAuth `invalid_grant`;
  isto permanece dependencia externa, nao sucesso alegado.
- Contadores nao mudam: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder; fila operacional
  `5/8 = 62.5%`. Estabilidade de release nao elimina pendencias de produto ou
  validacao em plataforma real.

## Recheck pos-tag v0.9.11 - 2026-07-19

- `master` esta em `03d71dad`, doze commits apos a tag anotada `v0.9.11` em
  `84462d4`. A tag ja existe nos dois remotos e nao foi movida ou recriada;
  uma proxima publicacao precisa de versao e tag distintas.
- O build canonico `dev` passou. O Ninja repetiu `premature end of file;
  recovering`, ruido do cache local que nao recebe credito como validacao.
- CTest sequencial atual passou `609/609` em `72.84 s`, inclusive os oito
  nomes do log historico com falha: analytics, derivadas, full rescan e Qt
  presentation. O log de `8/564` nao reproduz neste HEAD e nao e bloqueador
  ativo.
- Semgrep passou `24` regras em `512` arquivos; Gitleaks passou `714` commits;
  TruffleHog no historico Git terminou com zero segredo verificado ou
  desconhecido. Um scan filesystem separado terminou `183`, com exemplos em
  `.deepsec` local, XLSX historicos e erros de leitura de binarios; ele nao e
  registrado como gate verde nem como segredo de produto.
- Contadores permanecem: plano `99.0/100`, divida `13/14 = 92.9%`, legado
  `0.0/100` placeholder e fila operacional `5/8 = 62.5%`.

## Contraste runtime do setor rapido - 2026-07-19

- [RESOLVED-LOCAL] O Label `Setor:` de `PagerQuickFilters` usava
  `Theme.accent` direto sobre o `Theme.surface` declarado por `SearchAndPager`.
  O RED runtime encontrou contraste abaixo de AA em 17 das 39 paletas, de
  `2.23:1` a `4.21:1` nos casos que falharam.
- O patch de producao e uma unica cor contextual:
  `Theme.readableText(Theme.surface, Theme.accent)`. Ele preserva accent onde
  ja atende AA e seleciona texto legivel nos demais temas, sem mudar geometria,
  handlers, paletas ou tokens globais.
- O contrato instancia `PagerQuickFilters` real sob `Theme.surface`, le a cor
  resolvida do Label e percorre exatamente as 39 chaves de `Theme.palettes`.
  O primeiro review encontrou P1 de proxy sintetico e P2 de conjunto incompleto;
  ambos foram corrigidos antes do GREEN. Review Terra ultra final: `SEM FINDINGS`.
- Validacao local: diff, qmlformat, qmllint, clang-format, Semgrep QML (5
  regras), Semgrep C/security (2 regras) e detect-secrets passaram.
  `ssa_qml_advanced_popup_tests` GREEN `1/1` em `1.99 s`; `all_qmllint`
  passou. O warning Ninja de cache nao conta como validacao adicional.
- [PARTIAL-LOCAL] `QML-AA-BINDING-SMOKE` agora cobre este consumidor runtime;
  os outros sete consumidores permanecem pendentes de smokes dedicados.
  Contadores sem inflacao: plano `99.0/100`, divida `13/14 = 92.9%`, legado
  `0.0/100` e fila `5/8 = 62.5%`.

## Sincronizacao remota restaurada - 2026-07-18

- `origin/master` e `bitbucket/master` foram confirmados ate `09cc0c4`, que
  publica o contrato executavel Windows/UNC. A tag anotada
  `v0.9.11^{}` tambem foi confirmada em `84462d4` nos dois remotos.
- Os registros anteriores de OAuth `invalid_grant` permanecem historicos para
  auditoria, mas nao bloqueiam a publicacao atual.

## Recovery de selecao externa apos journal - 2026-07-18

- [IMPLEMENTED-LOCAL] [IMPORT-EXTERNAL-JOURNAL-REPLAY] Uma selecao externa
  distinta podia ser staged e descartada quando a chamada encontrava um
  journal de consolidacao anterior e o retomava com sucesso. O retorno era
  sucesso de recovery, mas o lote selecionado nao chegava ao SQLite nem a
  `processadas`.
- O workflow agora compara apenas candidatos com o mesmo nome de origem ao
  snapshot staged pendente. A igualdade usa tamanho e bytes completos,
  observa cancelamento e falha fechada em erro de I/O. A copia que representa
  o replay e removida; os demais snapshots ja staged seguem para importacao
  sem segundo copy ou restage.
- Os contratos distinguem tres casos: arquivo externo distinto continua apos
  recovery; o mesmo snapshot externo nao repete nem gera `duplicate_conflict`;
  e arquivo com o mesmo nome, mas bytes alterados, continua como lote novo.
  Todos exigem journal vazio e `PRAGMA integrity_check = ok`.
- Validacao local: RED do replay falhou `1/1` com `duplicate_conflict`; GREEN
  focal passou `7/7` em `0.51 s`; familia importacao/SQLite passou `196/196`
  em `7.60 s`. `git diff --check`, clang-format, cppcheck, Semgrep (`11`
  regras, zero finding) e detect-secrets passaram. clang-tidy nao mostrou
  warning novo; os cinco avisos restantes sao baseline do lock/construtor.
  Review Terra ultra final: `SEM FINDINGS`.
- O target `ssa_integration_tests` ligou com sucesso. O Ninja repetiu
  `premature end of file; recovering`; isto continua ruido do cache de build,
  sem credito adicional de validacao e sem refactor fora deste slice.
- Sem schema, layout ou API publica nova. Contadores sem inflacao: plano
  original `99.0/100`; divida nova `13/14 = 92.9%`; backlog legado `0.0/100`
  placeholder; fila operacional `5/8 = 62.5%`.

## Prazo SQLite configuravel no importador SAM - 2026-07-18

- [IMPLEMENTED-LOCAL] [SAM-SQLITE-BUSY-WAIT] `SamImportRequest` agora recebe
  `sqliteBusyWait`, com o default ja usado por `ImportExecutionOptions` de
  3000 ms. Antes, SAM fixava o prazo durante o recovery e descartava a
  configuracao ao iniciar a escrita.
- A faixa e o passo existentes sao validados antes de staging ou locks. A mesma
  opcao segue para a retomada do journal e para a escrita do lote. O refresh
  atual continua no default; expor configuracao de usuario e outro slice, sem
  misturar GUI, schema ou confiabilidade de importacao.
- RED: 1 ms era aceito e 0 ms ainda aguardava lock. GREEN: tres contratos
  causais passaram `3/3` em `0.61 s`: rejeicao antes de staging, escrita
  bloqueada e recovery de journal bloqueado. A familia importacao/SQLite
  passou `207/207` em `65.19 s`, com fonte, journal e integridade preservados
  antes do retry bem-sucedido.
- `git diff --check`, clang-format, cppcheck, Semgrep (11 regras, zero
  finding) e detect-secrets passaram. clang-tidy nao trouxe warning novo; os
  cinco avisos sao baseline. Review Terra ultra final: `SEM FINDINGS`.
- O target de integracao ligou, mas Ninja repetiu `premature end of file;
  recovering`; isto e ruido de cache e nao recebe credito de validacao.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder; fila operacional
  `5/8 = 62.5%`.

## TypeScale em controles compartilhados - 2026-07-18

- [PARTIAL-LOCAL] [TYPESCALE-POINTSIZE] O primeiro corte troca somente
  `ActionButton`, `AppComboBox`, seu delegate e `AppSpinBox` para o novo
  `Theme.fontPointSizeBody` de 9 pt. `Theme.controlHeight` continua minimo;
  a altura implicita cresce apenas quando o conteudo exige.
- O contrato QML instancia os tres controles com `Agypq`, abre o popup e exige
  `QFont.pointSizeF() > 0`, ausencia de `pixelSize` e area disponivel para o
  contentItem. Em 1.5x, ele tambem prova que o delegate herda o novo tamanho e
  continua sem clipping.
- RED final falhou na fonte em pixels. GREEN: `ssa_qml_theme_gallery_tests`
  passou `1/1` em `0.79 s`, incluindo 12 paletas em 960 e 720 px. Capturas
  offscreen de `ayu-light` e `flexoki-dark` foram inspecionadas sem clipping
  ou desalinhamento observavel.
- Formato, qmllint, Semgrep QML (5 regras, zero finding), Semgrep C (2 regras,
  zero finding) e detect-secrets passaram. O hook QML original nao podia
  escrever em `~/.semgrep` no sandbox; a mesma policy passou com `HOME` em
  `/private/tmp`. Review Terra ultra encontrou P2 no delegate 1.5x, corrigido;
  re-review final `SEM FINDINGS`.
- Nao e migracao global: AppTextField, AppCheckBox, filtros, cards e os demais
  QML continuam em pixel ou precisam de contrato proprio. Falta validar fonte
  grande em macOS, Windows e Linux reais. O warning Ninja de cache nao recebe
  credito de validacao.
- Contadores sem inflacao: plano `99.0/100`, divida `13/14 = 92.9%`, legado
  `0.0/100` placeholder e fila `5/8 = 62.5%`.

Proxima atividade unica: selecionar a maior pendencia local antiga com
evidencia suficiente, sem alegar que o TypeScale parcial conclui os 36 QML.

## Contrato externo Windows/UNC preparado - 2026-07-18

- [IMPLEMENTED-LOCAL] O CTest `ssa_windows_unc_import_contract` esta pronto
  para Windows real e share SMB gravavel. Ele fica fora da descoberta normal
  (`TEST_SPEC "~[windows-unc]"`) e so e registrado quando
  `SSA_ENABLE_WINDOWS_UNC_CONTRACT=ON` e
  `SSA_WINDOWS_UNC_TEST_ROOT` foi definido durante a configuracao. Ativar sem
  root falha no CMake de modo explicito; com a opcao desligada, o CMake apenas
  informa que o contrato externo esta desabilitado.
- O caso `[windows-unc]` rejeita namespaces locais `//?/` e `//./`, exige
  servidor e share, cria somente um filho temporario owned com segmento
  Unicode, executa dois full rescans no corpus e DB do share, valida
  `PRAGMA integrity_check`, linha publicada e move para `processadas`. Entre
  os rescans ele segura o `.ssa_import.lock` do corpus e exige
  `import_already_running` com a fonte preservada. Isto prova contencao na
  mesma maquina/share, nao entre hosts SMB.
- As fixtures XLSX agora passam caminhos por `qt::toUtf8()` ao miniz, inclusive
  padding e workbook multi-sheet. O backend miniz MSVC converte UTF-8 para
  wide; o teste deixa de depender de `std::filesystem::path::string()` e pode
  realmente atingir o filho Unicode.
- Validacao local: diagnostico e implementacao Terra ultra; review inicial
  encontrou dois P1 e dois P2 (registro ausente, namespace local, Unicode e
  RAII), todos corrigidos; re-review final `SEM FINDINGS`. `git diff --check`,
  clang-format, Semgrep (2 regras, 0 finding) e detect-secrets passaram. CMake
  `dev` e `ssa_integration_tests` compilaram; CTest focal de rescan/lock
  passou `5/5` em `0.16 s`, e o gate de paths Unicode/multi-sheet passou
  `4/4` em `0.36 s`. `ctest -N -R '^ssa_windows_unc_import_contract$'`
  retornou zero no macOS, como esperado: este resultado nao e prova Windows.
- O Ninja emitiu `premature end of file; recovering` e refez o target durante
  os builds. O link terminou com sucesso, mas o aviso de cache de build nao e
  tratado como validacao adicional nem como defeito de importacao provado.
- [PUBLISHED] O commit `09cc0c4` foi publicado em `origin/master` e
  `bitbucket/master`; os dois refs e o HEAD local foram conferidos no mesmo
  SHA, sem divergencia. O hook staged passou clang-format, cmake-format,
  Gitleaks, detect-secrets e TruffleHog.
- Execucao externa ainda pendente, sem credito: Windows 11 real, por exemplo
  PowerShell `$env:SSA_WINDOWS_UNC_TEST_ROOT='\\server\share'; cmake --preset dev
  -DSSA_ENABLE_WINDOWS_UNC_CONTRACT=ON; cmake --build --preset dev --target
  ssa_integration_tests; ctest --preset dev -R
  '^ssa_windows_unc_import_contract$' --output-on-failure`. O comando deve
  registrar versao Windows/SMB, share, resultado e erros Win32. Corrida entre
  hosts, reparse/junction, crash/recovery, troca TOCTOU e exportacao real de
  grafo seguem fora deste contrato.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100`; fila operacional `5/8 = 62.5%`.
  O aceite de `IMPORT-STAGER-HUB` continua externo.

Proxima atividade unica: executar `ssa_windows_unc_import_contract` em Windows
11 com share UNC gravavel e registrar a evidencia de plataforma antes de
considerar o ultimo item de importacao aceito.

## Contraste AA contextual QML - 2026-07-18

- [RESOLVED-LOCAL] [THEME-PY-AA] `Theme.readableText(background, preferred)`
  preserva a cor semantica preferida somente quando ela ja atende WCAG AA
  `4.5:1`; senao tenta os candidatos existentes e escolhe preto ou branco
  quando necessario. As paletas `*py` continuam importadas ipsis litteris.
- Os foregrounds de combo, botoes, tabs, atalhos, seletor de tema, tabela,
  analytics e Sobre agora usam o contexto efetivo. O link do grafico tambem
  recalcula contra `surface` no hover, em vez de assumir `panel`.
- O RED/GREEN percorre 39 paletas (26 nativas e 13 `*py`) e 37 pares de estado
  contextualizados. O CTest focal passou `1/1` em `2.30 s`; o fechamento QML
  passou `5/5` em `6.49 s` (popup, analytics, janela analytics, galeria e
  Sobre). `all_qmllint`, qmlformat, clang-format, Semgrep e detect-secrets
  passaram. cppcheck nao seleciona arquivo de teste e nao houve C++ de
  producao neste slice.
- Review Terra ultra encontrou dois P2 reais: contraste do hover do link e
  restauracao incorreta do singleton de tema em falha. Ambos foram corrigidos;
  re-review final: `SEM FINDINGS`. P3 residual: o contrato testa a funcao e
  os pares declarados, mas nao instancia os oito bindings de consumidores.
- A migracao `TYPESCALE-POINTSIZE` permanece pendente: ha 100 usos de pixel
  em 36 QML, varios com altura fixa. Troca mecanica alteraria densidade e pode
  causar clipping; exige slice proprio e validacao macOS/Windows/Linux real.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder; fila operacional
  `5/8 = 62.5%`. Este fechamento e hardening adicional fora dos denominadores.

## Recheck UX-GRID-ALIGN - 2026-07-18

- A reproducao offscreen atual em `1580x940` nao reproduziu o desalinhamento
  reportado: Macro e Reprogramacoes mostram baseline, padding e altura visual
  coerentes com os cards de texto adjacentes. Nenhum QML foi alterado.
- O CTest `ssa_qml_advanced_popup_tests` passou `1/1` em `1.58 s`. A lacuna
  restante e apenas cobertura automatica que compare as tres geometrias no
  mesmo `Flow`. O item exige reproducer no monitor afetado ou contrato
  geometrico antes de qualquer mudanca de layout.

## Analytics atomico na importacao incremental - 2026-07-18

- `44411da` adiciona um contrato de workflow que injeta schema invalido em
  `activity_analytics_point`. A falha e identificada como analytics, retorna
  `Failed` e mantem fonte, SSA anterior, journal e tabelas analytics sem
  publicacao parcial. Isto prova rollback do mesmo commit SQLite, nao apenas
  uma falha generica anterior.
- O target manual real `run_activity_analytics_benchmark_250k` agora percorre
  `rescan -> finishWithAnalytics`: base de 250000 linhas, no-op com o mesmo
  XLSX fisico restaurado e delta de uma linha. A rodada validada mediu base
  `12239.400 ms`, no-op `10338.718 ms`, delta `2514.052 ms`; contrato
  `250000 -> 250000 -> 250001`, seis snapshots, nove pontos e agregado SPG
  `250001`. O pico adicional de RSS foi `121978880` bytes, abaixo de 256 MiB.
- O gate focado passou `28/28` em `14.67 s`; o benchmark manual passou sem
  limite temporal artificial. Formatacao, cppcheck, Semgrep e detect-secrets
  passaram; clang-tidy saiu zero sem finding novo, mas ainda imprime avisos
  preexistentes de Catch2 e do arquivo de teste.
- Estado separado: implementado e validado localmente no commit `44411da`;
  Bitbucket confirmou `master` em `44411da`; GitLab `origin` recusou push por
  OAuth `invalid_grant`, dependencia externa sem publicacao alegada.
- Diagnostico Terra ultra mapeou import writer, Derivadas, Maintenance e a
  publicacao do rescan por backup. Um ledger seguro seria sidecar no mesmo DB,
  com triggers para DML externo, bootstrap fail-closed e copia junto com o DB
  temporario; `capture` ainda teria de executar para data e integridade.
- Nenhum schema foi aprovado: o fingerprint isolado historico de 250 mil
  linhas mede `239.838 ms`, cerca de `2.32%` do no-op real de `10338.718 ms`.
  Triggers por linha, bootstrap legado e contratos de rollback/publicacao
  ainda nao foram medidos. O ganho marginal nao justifica este risco agora.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder; fila operacional
  `5/8 = 62.5%`.

Proxima atividade unica: mapear a prova Windows/UNC real do ultimo item de
confiabilidade de importacao, sem alterar schema ou staging especulativamente.

## UX-NAV: cadeia de derivadas fixa durante navegacao - 2026-07-18

- Branch `master`; codigo no HEAD `08991b6`. A confirmacao externa ainda e
  pendente neste ponto do registro. GitLab `origin` segue bloqueado por OAuth
  `invalid_grant`, dependencia externa separada da validacao local.
- A selecao da tabela e a unica dona do snapshot da cadeia. `setRecord()` cria
  a geracao e carrega filhas diretas; navegar por relacao atualiza somente o
  registro exibido. Nova selecao, lookup externo e limpar detalhes invalidam
  o snapshot e resultados async antigos por ID/geracao.
- O carregamento de filhas e o de registro possuem lanes independentes. Cada
  lane conserva seu erro; `relationError` prioriza Record. Assim a conclusao
  de filhas nao apaga `SSA nao encontrada`, e a navegacao bem-sucedida nao
  apaga uma falha de filhas. Navigator e janela de grafo exibem erro antes de
  loading. O QML encaminha indice, nao SSA, preservando a linha correta quando
  uma cadeia contem a mesma SSA duas vezes.
- REDs adicionados: cadeia era reconstruida, filhas pendentes eram canceladas,
  indice duplicado era perdido, e os dois sentidos de erro concorrente. O
  primeiro review Terra ultra encontrou P1 de cancelamento de filhas e P2 de
  indice; o segundo encontrou P2 de erro global. Todos foram corrigidos;
  re-review final Terra ultra `SEM FINDINGS`.
- Gate local: `git diff --check`, clang-format, cppcheck, clang-tidy,
  qmlformat, qmllint, detect-secrets e Semgrep (`11` regras, zero finding)
  limpos. Build dos dois targets afetados; CTest
  `ssa_qt_presentation_tests` e `ssa_qml_advanced_popup_tests` passou `2/2`
  em `14.87 s`; `all_qmllint` passou. O hook do commit passou clang-format,
  qmlformat, qmllint, Semgrep QML, Gitleaks, detect-secrets e TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder e fila operacional
  `5/8 = 62.5%`. UX-NAV e hardening adicional fora desses denominadores.
- Diagnostico da sync completa de derivadas: C++ persiste apenas
  `ssa_table.derivada_de` e filhas diretas. A referencia externa usa
  procedencia, matriz, closure, resumo e sync runs. Antes de DDL ou fluxo novo
  faltam cinco politicas de produto: prioridade de fonte, multiparent, ciclos,
  orfaos e desativacao de arestas ausentes. Nenhuma mudanca de schema foi feita.

Proxima atividade unica: executar o gate completo de estabilizacao do candidato
`0.9.11`; criar a tag somente se a suite e as verificacoes de release passarem.

## Checkpoints causais privados de parsing e merge de derivadas - 2026-07-18

- Branch `master`; codigo em `77fd3e0`, confirmado no Bitbucket. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`; isto e dependencia
  externa, nao falha de codigo nem publicacao confirmada no GitLab.
- Tres contratos deixaram de usar `wait_for(1/10ms)` como precondicao. O parser
  para no primeiro intervalo real de 4 KiB e o fluxo completo para logo apos a
  primeira edge mergeada; em ambos, o stop ocorre antes do watchdog e o banco
  precisa permanecer vazio e reutilizavel.
- `DerivadasImportTestAccess` existe somente em `tests/` e chama overloads
  privados por chamada. As APIs publicas de reader, merger e port voltaram as
  assinaturas anteriores; sem hooks o caminho normal nao cria semaforo, callback
  ou estado adicional. Nao houve mudanca de schema, SQL ou regra de derivadas.
- RED inicial confirmou API ausente. O primeiro review Terra ultra encontrou
  P2 de sinais expostos na API publica e P1 de merge fora do port; ambos foram
  corrigidos por um segundo RED com acesso privado. Dois re-reviews Terra ultra
  finais deram `SEM FINDINGS` para contrato e concorrencia.
- Gate local: diff, clang-format, cppcheck, clang-tidy, detect-secrets e
  Semgrep (`11` regras, zero finding) limpos; build de `ssa_integration_tests`;
  causal `3/3` em `0.08 s`, repeticao `90/90` em `1.90 s` e familia derivadas
  `22/22` em `0.49 s`. O hook do commit passou clang-format, Gitleaks,
  detect-secrets e TruffleHog. `clawpatch` continua indisponivel por CLI/modelo
  local antigo e seleciona arquivo externo ao slice; nao conta como review.
- Risco residual: o cancelamento continua cooperativo; `std::getline` nao pode
  interromper um read de SO ja bloqueado. Os checkpoints provam parse e merge,
  nao I/O externo bloqueado.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder e fila operacional
  `5/8 = 62.5%`. Nenhum credito funcional foi atribuido.

Proxima atividade unica: diagnosticar a pendencia legada de sincronizacao
completa de derivadas com regras de negocio e fonte externa, primeiro mapeando
contrato e dados reais sem alterar fluxo ou schema.

## Cancelamento causal de derivadas sob SQLite bloqueado - 2026-07-18

- Branch `master`; codigo em `6e818c0`, publicado no Bitbucket. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`; isto e dependencia
  externa, nao falha de codigo nem publicacao confirmada no GitLab.
- `SqliteDerivadasPort` agora aceita sinal opcional `busyEntered`, retido por
  `shared_ptr` e encaminhado apenas ao `SqliteBusyHandler` de escopo local. O
  caminho normal recebe sinal vazio; importacao de derivadas e limpeza de orfas
  preservam API, schema, SQL e regras existentes.
- Dois contratos deixam de usar `wait_for(50ms)`: cada um toma `BEGIN
  EXCLUSIVE`, aguarda o primeiro `SQLITE_BUSY` real, pede stop e faz ROLLBACK
  antes das assercoes. O watchdog de `500 ms` continua falha, nunca sucesso.
  Assim ambos provam cancelamento sob contencao e reuso posterior do banco.
- RED: o teste primeiro falhou por API ausente. O gate encontrou duas lacunas
  de portabilidade e elas foram corrigidas: clang macOS rejeitava inicializador
  redundante da struct aninhada com default `{}`, e `counting_semaphore` exige
  contagem inicial `0`. Review Terra ultra final: `SEM FINDINGS`.
- Gate local: diff, clang-format, cppcheck, clang-tidy, detect-secrets e
  Semgrep (`11` regras, zero finding) limpos. Build de `ssa_integration_tests`;
  contratos causais `2/2` em `0.07 s`, repetidos `60/60` em `2.57 s`; familia
  derivadas `22/22` em `0.61 s`; SQLite Repository `31/31` em `0.76 s`.
  `clawpatch` ficou indisponivel por CLI/modelo local antigo e selecionou
  arquivo externo ao slice; nao conta como review concluido. Hooks staged
  passaram clang-format, Gitleaks, detect-secrets e TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder e fila operacional
  `5/8 = 62.5%`. Nenhum credito funcional foi atribuido.

Proxima atividade unica: tornar causal o cancelamento durante parsing e merge
de derivadas, em corte separado do SQLite para nao criar falso acoplamento.

## Recovery de staged journalizado antes de rescan - 2026-07-18

- Branch `master`; codigo em `1d04ab0`, publicado no Bitbucket. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`; isto e dependencia
  externa, nao falha de codigo nem publicacao confirmada no GitLab.
- O `rescan()` agora retoma a consolidacao journalizada logo apos adquirir os
  locks e antes de `stageInputFiles()` limpar artifacts `.ssa-staged-*`. Assim,
  um crash entre o commit SQLite e o move de uma importacao externa preserva a
  copia owned ate o `ImportFileConsolidator` concluir o move idempotente.
- RED real: o contrato de processo passou a journalizar
  `.ssa-staged-crashed_123_0.xlsx`; no baseline, `rescan` apagava a fonte e
  falhava com `consolidation source and destination state is ambiguous` em
  `0.06 s`. O patch tambem remove `importIncrementalFiles`, private sem call
  site que so era alcancado pelo ramo de cancelamento antigo e quebrava a
  atomicidade do lote.
- Gate local: diff, clang-format, cppcheck, detect-secrets e Semgrep (`11`
  regras, zero finding) limpos; clang-tidy repetiu quatro avisos preexistentes
  de assinatura/copia fora do diff. Build de `ssa_integration_tests`; contratos
  diretos `5/5` em `0.47 s`; workflow `174/174` em `9.43 s`; recovery de crash
  repetido `20x` em `1.07 s`. Review Terra ultra final: `SEM FINDINGS`.
  `clawpatch` ficou indisponivel por CLI/modelo local antigo e selecionou
  arquivo externo ao slice; nao conta como review concluido. Hooks staged
  passaram clang-format, Gitleaks, detect-secrets e TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder e fila operacional
  `5/8 = 62.5%`. Nenhum credito funcional foi atribuido.

Proxima atividade unica: substituir a espera temporal de cancelamento sob lock
em `SqliteDerivadasPort` por sinal causal de `SQLITE_BUSY`, sem mudar regra de
derivadas ou schema.

## Handshake causal dos crash probes SQLite - 2026-07-18

- Branch `master`; codigo em `6bcf65e`, confirmado no Bitbucket. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`; isto e dependencia
  externa e nao equivale a falha de codigo ou a publicacao confirmada no GitLab.
- Os tres contratos de crash deixam de observar arquivos por polling e aguardam
  `READY\n` no stdout do probe. O probe grava o marker atomico com `QSaveFile`
  antes do token; o parser acumula bytes para tolerar leitura parcial, e o
  watchdog de 5 s continua apenas como falha, nunca como sucesso.
- Para `journal-delete-before-commit`, `pendingConsolidation()` usa writer sem
  instrumentacao; um segundo writer observado recebe `busyEntered` somente em
  `completeConsolidation`. A `jthread` publica `READY` apos a primeira
  contencao do DELETE/COMMIT e seu `stop_callback` libera o semaforo se o probe
  sair por erro, evitando deadlock no destrutor. O teste ainda exige `-journal`
  antes do kill.
- RED esperado: os tres contratos falharam sem protocolo `READY`. O primeiro
  review Terra ultra encontrou P1 de sinal possivel na leitura previa; foi
  corrigido pelo writer observado separado. Review final Terra ultra: `SEM
  FINDINGS`. `clawpatch` local ficou indisponivel por configuracao/modelo
  externo incompativel e nao conta como review concluido.
- Gate local: diff, clang-format, detect-secrets e Semgrep (`2` regras, zero
  finding) limpos; build de `ssa_integration_tests`; CTest causal `3/3` em
  `0.36 s`; workflow `174/174` em `7.38 s`; journal-before-commit repetido
  `10x` em `0.78 s`. Hooks staged passaram clang-format, Gitleaks,
  detect-secrets e TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` placeholder e fila operacional
  `5/8 = 62.5%`. Nenhum credito funcional foi atribuido.

Proxima atividade unica: priorizar por risco a proxima pendencia local de
confiabilidade de importacao/SQLite fora do denominador fixo, sem tentar simular
Windows/UNC ou inflar os contadores.

## Cancelamento causal do conversor legacy - 2026-07-18

- Branch `master`; codigo em `0172f2e`, confirmado no Bitbucket. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`, uma dependencia externa
  que nao invalida a evidencia local ou a publicacao no Bitbucket.
- O contrato de cancelamento do conversor deixa de observar o sidecar
  `.conversion-ready` em loop de `5 ms` por ate `10 s`. Um runner falso ja
  permitido pelo port cria a saida parcial, libera um `binary_semaphore` e
  aguarda o `stop_token` por `stop_callback`; o teste pede cancelamento apenas
  depois do checkpoint causal.
- O corte remove tambem o include e comportamentos de fake soffice sem uso. Ele
  prova boundary do conversor e cleanup de diretorio, nao spawn de filho OS;
  essa prova continua na suite de `SupervisedProcess`.
- Gate local: diff, clang-format, detect-secrets e Semgrep (`2` regras, zero
  finding) limpos. `clang-tidy` filtrado nao encontrou aviso nas linhas novas;
  o hook cppcheck nao seleciona testes. Build afetado passou; CTest legacy
  `4/4` em `0.08 s`; workflow completo `174/174` em `6.85 s`. Review Terra
  ultra final: `SEM FINDINGS`. Hooks staged passaram clang-format, Gitleaks,
  detect-secrets e TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`. Nenhum credito funcional foi atribuido.

Proxima atividade unica: trocar os tres loops de checkpoint dos crash probes
SQLite por sinal causal de processo ou filesystem, sem alterar recovery SQLite.

## Checkpoint causal de copia/staging - 2026-07-18

- Branch `master`; codigo em `79f8200`, confirmado no Bitbucket. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`; isto e dependencia
  externa, nao falha do codigo nem publicacao confirmada no GitLab.
- Sete contratos de staging deixaram de procurar `.part` em loop de `1 ms`.
  `CancelableFileCopy` agora expoe um hook opcional apos a primeira escrita
  aceita; `ImportFileStager` e o workflow o propagam sem comportamento extra
  quando o hook esta vazio. Os contratos exercitam cancelamento, troca/remocao
  de fonte, inventario parcial, importacao externa e cleanup com permissao.
- O RED de callback que lanca confirmou que a excecao escapava. Dois reviews
  Terra ultra classificaram o caminho como P2. A fronteira agora fecha e tenta
  remover o temporario, retornando `Failed` ou `CleanupFailed` com diagnostico;
  review final: `SEM FINDINGS`.
- Gate local: diff, clang-format, cppcheck, detect-secrets e Semgrep (`11`
  regras, zero finding) limpos. `clang-tidy` nao encontrou aviso novo no diff;
  ele repetiu avisos preexistentes de parametros no workflow. Build de
  `ssa_integration_tests` passou; RED `1/1` falhou como esperado; CTest focado
  `8/8` em `0.80 s`; workflow completo `174/174` em `8.65 s`. Hooks do commit
  passaram clang-format, Gitleaks, detect-secrets e TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`. Nenhum credito funcional foi atribuido.

Proxima atividade unica: substituir o polling de processo restante do conversor
legacy por handshake causal, sem mudar regras de importacao ou schema.

## Polling de staging e corpus lock removido - 2026-07-18

- Branch `master`; codigo em `1f99a90`, confirmado no Bitbucket. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`; isso nao reduz a
  validacao local nem equivale a publicacao no GitLab.
- O patch altera somente `SpreadsheetImportWorkflowPortTests.cpp`. Tres
  contratos agora aguardam `writerBusyEntered`, que ocorre apos staging e na
  primeira contencao SQLite real: cancelamento apos staging, cleanup owned com
  permissao negada e retencao do corpus lock durante rescan.
- Foram removidos os loops de `QElapsedTimer`/`msleep(5)` e o wait fixo de
  `50 ms`. As assercoes observaveis permanecem: workbook staged, cleanup,
  rollback SQLite e o `QLockFile` concorrente nao pode adquirir o corpus lock.
- O primeiro review Terra ultra encontrou P1: `snapshotLocked` so ocorre na
  publication, enquanto o rescan deste teste bloqueia no preflight. O patch
  foi corrigido para `writerBusyEntered`; reinspecao final: `SEM FINDINGS`.
- Gate local: diff, clang-format, detect-secrets e Semgrep limpos; cppcheck nao
  tinha arquivo de producao neste slice. Build de `ssa_integration_tests`
  passou. CTest focado `3/3` em `0.48 s`; workflow completo `174/174` em
  `7.72 s`. Hooks do commit passaram clang-format, Gitleaks, detect-secrets e
  TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`. Nenhum credito funcional foi atribuido.

Proxima atividade unica: substituir o proximo grupo de polling de staging
direto por sinal causal sem alterar o comportamento de producao.

## Sinais causais de contencao SQLite na importacao - 2026-07-18

- Branch `master`; HEAD de codigo `f62ea53`. Bitbucket confirma o mesmo hash.
  GitLab `origin` continua bloqueado por OAuth `invalid_grant`; o local esta
  `77` commits a frente de `origin/master`.
- `SqliteSsaImportWriter` agora retira precondicoes temporais com um sinal
  opcional de primeira callback busy, ownership por `shared_ptr` e retencao na
  `WriteSession::Storage`. O workflow separa `writerBusyEntered` de
  `snapshotLocked`, que so e publicado na fase de publication do SQLite Backup.
- Os contratos trocam esperas de `50/250/500 ms` por `try_acquire_for(1 s)`
  somente antes de cancelar ou liberar o lock. Watchdogs de termino continuam
  como limites de falha, nunca como sucesso.
- Review Terra ultra encontrou dois P2: permit antigo no cleanup e permit duplo
  entre `SQLITE_BUSY` e `SQLITE_LOCKED`. Ambos foram corrigidos; o handler e o
  fallback agora compartilham um unico `atomic_flag`. Review final: `SEM FINDINGS`.
- Gate local: diff, clang-format, cppcheck, detect-secrets e Semgrep limpos;
  clang-tidy sem erro novo. Build afetado passou. CTest causal `7/7` em `0.56 s`,
  workflow completo `174/174` em `7.96 s` e familia SQLite Repository `31/31`
  em `0.72 s`. Hooks do commit passaram clang-format, Gitleaks,
  detect-secrets e TruffleHog.
- Contadores sem inflacao: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`.

Proxima atividade unica: substituir o polling de staging e corpus lock restante
nos contratos de importacao por sinais causais equivalentes.

## Benchmark da formatacao de pagina - 2026-07-18

- `ce90132` adiciona `ssa_table_page_format_benchmark`: fixture preconstruida
  de `500` linhas por `12` colunas com texto, inteiros e datas, isolando o
  formatter do I/O e da GUI. O JSON local nao versionado fica em
  `build/dev/table-page-format-benchmark-30.json`.
- Em `30` amostras de `10` iteracoes, por pagina, wall p50/p95 foi
  `2.93215/3.3340834 ms` e CPU p50/p95 foi `2.9244/3.2649 ms`. Como o formatter
  ja roda no worker, a medicao nao justifica refactor lazy ou mudanca de layout.
- Gate local: diff, clang-format, cmake-format, cppcheck, clang-tidy,
  detect-secrets e Semgrep limpos; build do target e smoke CTest `1/1` em
  `0.04 s`. Review Terra ultra: `SEM FINDINGS`. O `clawpatch` local falhou por
  configuracao/versionamento do proprio Codex e nao conta como validacao.
- Branch `master`; HEAD `ce90132`; Bitbucket confirma o mesmo hash. GitLab
  `origin` continua bloqueado por OAuth `invalid_grant`; o local esta `74`
  commits a frente de `origin/master`.
- Contadores sem mudanca: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`.

Proxima atividade unica: propagar sinal causal de `sqliteBusyWait` pelos
contratos de importacao e rescan que ainda dependem de espera temporal.

## Reconciliacao do macro report - 2026-07-18

- O finding P4 de agregacao `map<ReportKey,set<string>>` esta obsoleto no HEAD:
  `SqlQueryBuilder::buildExecutadasReport()` delega ao analytics SQL, que usa
  `COUNT(DISTINCT "ssa_number")`; `SqliteSsaRepository` apenas materializa o
  resultado. Nao existe agregacao equivalente em memoria para migrar.
- Evidencia local: os CTests de SQL compilado, propagacao completa de filtros e
  resultado agrupado passaram `3/3` em `0.13 s`. Nenhum benchmark novo foi
  criado, pois o gargalo descrito ja nao existe; escala futura segue sem medicao
  especifica de macro report.
- Contadores sem mudanca: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`.

Proxima atividade unica: medir o custo real de formatacao eager da tabela antes
de considerar cache lazy ou qualquer mudanca de GUI.

## Determinismo causal do repositorio SQLite - 2026-07-18

- Branch `master`; HEAD de codigo `28700b3`. Bitbucket confirma o mesmo hash;
  GitLab `origin` segue bloqueado por OAuth `invalid_grant`. O local esta `71`
  commits a frente de `origin/master`.
- `28700b3` remove o polling do arquivo de lock e as esperas de `10/50 ms` dos
  contratos de derived summary e query lenta. Os testes agora aguardam eventos
  reais: `QLockFile` adquirido, primeira callback busy e primeira callback de
  progress SQLite.
- A seam opcional mantem ownership por `shared_ptr<counting_semaphore>`; a
  construcao normal usa sinais nulos e preserva comportamento. Review Terra
  encontrou P2 de lifetime/overflow e o gate encontrou P1 de tipo entre handlers
  e repositorio; ambos foram corrigidos. Review final Terra ultra: `SEM FINDINGS`.
- Gate: diff, clang-format, cppcheck, detect-secrets e Semgrep (`62` regras)
  limpos. `clang-tidy` so reportou aviso preexistente de `stop_token` por valor.
  Build afetado passou; familia SQLite Repository completa passou `31/31` em
  `0.68 s`. Hooks do commit passaram formatacao, Gitleaks, detect-secrets e
  TruffleHog.
- Contadores sem mudanca: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`. Este e hardening adicional, sem credito retroativo.
- Restam waits de `50 ms` em maintenance e derivadas, fora deste corte e sem
  regressao demonstrada. Nenhum timeout foi tratado como sucesso.

Proxima atividade unica: medir e provar equivalencia do macro report com
`COUNT(DISTINCT)` em SQL antes de migrar o calculo em memoria.

## Indice status-last de producao - 2026-07-18

- Branch `master`; HEAD de codigo `4396e1c`. Bitbucket foi confirmado no mesmo
  hash; GitLab `origin` continua bloqueado por OAuth `invalid_grant`. O local
  esta `69` commits a frente de `origin/master`.
- `SqlQueryBuilder` e `SqliteSsaImportWriter` compartilham a mesma expressao
  `CASE` e o writer instala o indice DESC somente quando a tabela possui
  `numero_ssa` e `situacao`. Bancos existentes recebem o indice na proxima
  escrita; nao houve bump de schema nem migracao durante leitura.
- O contrato cobre DDL exato, plano sem `TEMP B-TREE`, recriacao apos remocao,
  tabela legacy customizada e colisao de nome fail-closed que preserva o indice
  de outra tabela. O primeiro review Terra ultra encontrou dois P2 de ownership
  e cobertura legacy; ambos foram corrigidos. Review final Terra ultra:
  `SEM FINDINGS`.
- Gate: `git diff --check`, clang-format, cppcheck e detect-secrets passaram;
  Semgrep executou `62` regras com zero finding. `clang-tidy` so reportou tres
  avisos preexistentes fora do diff. Build dos targets afetados passou; CTest
  direto passou `38/38` em `1.14 s`, incluindo importacao, query e smoke do
  benchmark.
- Contadores sem mudanca: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`. O ganho nao recebe credito novo fora dos denominadores fixos.
- Risco residual: o indice so aparece apos a proxima escrita e nao substitui
  prova de filtros arbitrarios, GUI completa ou profiling valido do prefetch.

Proxima atividade unica: substituir polling temporal de `SqliteRepositoryTests`
por sinais causais de lock, busy e progress do SQLite.

## Benchmark SQLite status-last - 2026-07-18

- O commit `1d950e3` entrega o harness isolado de `status-last`: fixture de
  `250000` linhas e `30` amostras. O JSON local nao versionado esta em
  `build/dev/sqlite-status-last-benchmark-30.json`.
- O plano baseline registra `TEMP B-TREE`; o plano indexado usa
  `idx_ssa_table_status_last_numero_ssa_desc` e nao registra `TEMP B-TREE`.
  Os vetores de resultados foram identicos nas duas variantes.
- Wall p50/p95 baseline: `41.260958/45.094667 ms`; indexado:
  `0.0235/0.030125 ms`. CPU p50/p95 baseline: `41.184/44.742 ms`; indexado:
  `0.024/0.031 ms`.
- Escopo: fixture SQL isolada. Ainda nao existe indice de producao, nem prova
  para filtros arbitrarios ou GUI completa. Gates estaticos limpos; smokes
  `2/2` em `0.37 s`; review final Terra ultra: `SEM FINDINGS`.
- Publicacao historica: Bitbucket `0/0` em `1d950e3`; GitLab `origin` segue
  bloqueado por OAuth `invalid_grant`; o local estava `67` commits a frente de
  `origin/master`.
- Contadores sem mudanca: plano original `99.0/100`; divida nova
  `13/14 = 92.9%`; backlog legado `0.0/100` como placeholder; fila operacional
  `5/8 = 62.5%`.

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

## Fechamento local de tooling

- Branch `master`; HEAD de codigo `2e2476b`. Bitbucket foi confirmado com
  divergencia `0/0`. O GitLab `origin` continua bloqueado por OAuth
  `invalid_grant`; o HEAD local esta 63 commits a frente de `origin/master`.
- Plano original: `99.0/100`; profiling valido do prefetch permanece o unico
  ponto sem aceite e esta bloqueado pela ferramenta local.
- Divida nova: `13/14 = 92.9%`; backlog legado: `0.0/100` como placeholder sem
  denominador; fila operacional: `5/8 = 62.5%`.
- `CMAKE_OSX_SYSROOT=macosx` agora esta nos cinco presets-raiz (`dev`,
  `release`, `dev-asan`, `dev-tsan` e `dev-cov`); `dev-arrow` herda `dev`.
  O compile database passou a registrar SDK macOS 26.5, sem alterar Linux ou
  Windows.
- `clang-tidy` direto, sem argumentos extras de sysroot, passou em `2.50 s`.
  O build de `ssa_infra` passou 58 passos em `5.55 s`; o target de integracao
  passou 160 passos em `13.19 s`; o mapper passou `15/15` em `0.44 s`.
- Semgrep 1.170 focado em `SpreadsheetImportWorkflowPortTests.cpp` (6,815
  linhas) passou em `2.92 s`: duas regras atuais elegiveis, zero finding e zero
  timeout. `SEMGREP-WIDE-NARROW-TIMEOUT` foi refutado localmente: a alegacao de
  `63554b9` nao tem IDs, YAML, fixture ou log. Nenhuma fixture foi dividida.
  A equivalencia exata com Semgrep 1.169 do CI nao foi validada porque o socket
  Docker local esta ausente; isso permanece dependencia externa, nao sucesso.

Proxima atividade unica: executar a prova Windows/UNC em plataforma real para
importacao e grafo. Profiling valido do prefetch continua bloqueado; handles,
PowerShell e packaging tambem exigem plataforma real.

## Fechamento de determinismo e filename timestamp

- Branch `master`; HEAD de codigo `be20b99`. Bitbucket foi confirmado no mesmo
  hash com divergencia `0/0`. O GitLab `origin` continua bloqueado por OAuth
  `invalid_grant`; antes do commit documental, o HEAD local estava 61 commits a
  frente de `origin/master`.
- Plano original: `99.0/100`. O unico ponto sem aceite continua sendo 1.0 de
  profiling valido do prefetch, bloqueado pela ferramenta local.
- Divida nova: `13/14 = 92.9%`. `TEST-DETERMINISM-DELTA` e
  `IMPORT-FILENAME-TIMESTAMP-SCAN` receberam aceite binario; resta somente
  `IMPORT-STAGER-HUB`, dependente de Windows/UNC real.
- Backlog legado: `0.0/100`, mantido apenas como placeholder historico sem
  denominador aceito.
- Fila prioritaria operacional: `4/8 = 50.0%`. Busy wait, gate de schema,
  determinismo e benchmark de filename estao resolvidos. Restam profiling,
  tooling local e dois pacotes de prova multiplataforma.
- Os commits de `785c73e` ate `bcb7980` substituem sleeps, polling e esperas
  artificiais por barreiras causais, sinais, clocks injetados, hooks
  post-commit e gates terminais. A matriz focada passou `26/26` em `28.38 s`.
  O mapper passou `30/30` em `2.21 s`; seu wait curto foi preservado como janela
  temporal deliberada. Waits residuais observam filesystem, processos, SQLite,
  timers ou cancelamento reais. Nenhum timeout foi aceito como sucesso.
- O CTest sequencial passou `595/595` em `66.86 s`. Depois, o novo smoke do
  benchmark elevou o registro configurado para 596; portanto, o full nao inclui
  esse smoke. O gate focado do benchmark passou `3/3` em `0.41 s` e o recheck
  passou `3/3` em `0.13 s`.
- `be20b99` mede timestamp de filename em 30 processos, 12,000 parses e
  `2.00 s`. O p95 foi `783.3 ns` para ISO no inicio e no maximo
  `178269.15 ns`, ou `0.178 ms`, no corpus adversarial. O parser atual e O(n);
  a medicao nao justifica otimizacao.
- Security ampla atual: Semgrep `509` arquivos, `24` regras e zero finding;
  Gitleaks `1.42 GB` e zero leak; TruffleHog `9,226` chunks e zero segredo
  verificado ou desconhecido.

Proxima atividade unica: canonizar clang-tidy no macOS com sysroot e reproduzir
os IDs das regras Semgrep. O profiling permanece bloqueado pela ferramenta;
depois do tooling, executar as provas externas Windows/UNC, handles,
PowerShell e packaging.

## Historico anterior: Marco Activity, importacao parametrizada e SQLite

- Branch: `master`.
- HEAD de codigo validado: `b2369ac`, precedido por `7291082`, `0025d44` e
  `7f396b0`. Bitbucket foi confirmado em `b2369ac`. O push GitLab `origin`
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
  Esse valor e somente o placeholder historico exigido pelo controle anterior;
  nao representa 0% de execucao porque nunca houve denominador aceito.
- Fila prioritaria operacional: `2/8 = 25.0%`. Resolvidos: propagacao integral
  de busy wait e gate transacional de schema. Pendentes: determinismo, benchmark
  de timestamp em filename, profiling do prefetch, tooling clang-tidy/Semgrep,
  Windows/UNC de importacao/grafo e handles/PowerShell/packaging reais.
- Os tres itens exatos que seguram a divida nova em `11/14` sao
  `IMPORT-STAGER-HUB`, `IMPORT-FILENAME-TIMESTAMP-SCAN` e
  `TEST-DETERMINISM-DELTA`. Fechar os dois itens locais move esse contador para
  `13/14 = 92.9%` e a fila operacional para `4/8 = 50.0%`.
- Estado separado: `610fbf3` prova o snapshot WAL do rescan. Em `7f396b0`, os
  targets afetados compilaram, CTest sequencial passou `588/588` em `78.30 s`,
  o conversor passou 20 repeticoes, SAM passou 20 suites completas e Details
  passou 10 repeticoes. Semgrep amplo, Gitleaks e TruffleHog ficaram limpos.
  O commit esta publicado e comprovado no Bitbucket. GitLab e plataformas nao
  locais permanecem prova externa pendente.

### Fechamento local de 2026-07-18

- `91c60a1` endurece Activity Analytics: metadata, storage classes, data/semana,
  revisao ativa, pontos e disponibilidade falham fechado; reparo da semana
  corrente e deterministico e preserva configuracao valida.
- `eede38e` adiciona `ImportExecutionOptions`: chunks de 1 a 1,000 linhas e
  SQLite busy wait de 0 a 3,000 ms em passos de 5 ms, validados antes de staging
  e lock e propagados por full, incremental, external e recovery.
- `7291082` fecha a propagacao que ainda estava incompleta: `sqliteBusyWait`
  agora governa lookup e cleanup do journal, resume, external, rescan, SAM e
  publicacao. Callers diretos preservam default de 250 ms e cancelamento ja
  solicitado limita o lookup a no maximo 250 ms.
- `b2369ac` fecha `SQLITE-SCHEMA-VERSION-GATE`. Writer, recovery lookup,
  cleanup de journal e validator rejeitam versoes futuras antes de DDL/DML ou
  movimento de arquivo. Legado zero e aceito, recebe `schemaVersion()==1`
  dentro da mesma write transaction e rollback/crash preservam zero. O
  validator usa uma read transaction unica para versao, estrutura e registros.
- O RED de recovery moveu o arquivo e retornou sucesso sob `user_version=2`.
  Depois do patch, os contratos principais passaram `80/80`, cleanup direto
  passou `30/30` e a familia SQLite/importacao passou `45/45` em `2.87 s`.
  A primeira matriz integral terminou `590/594`; os quatro casos falhos
  passaram isolados e `40/40` repeticoes. A repeticao integral final passou
  `595/595` em `75.81 s`.
- O RED de recovery terminou cedo aos 250 ms apesar do request de 3,000 ms.
  Depois do patch, preflight, publicacao e cleanup passaram 30 execucoes; a
  familia ampliada de journal, cancelamento, WAL, SAM e rescan passou `15/15`
  em `2.67 s`. Os oito nomes do registro antigo passaram `8/8` em `9.77 s` na
  matriz atual de 588 casos.
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
  `10.09 s` no registro atual. A matriz configurada agora possui 588 casos.
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
- Snapshot WAL: `610fbf3` mantem o snapshot antigo para um leitor em transacao
  durante a publicacao pelo Backup API e torna o novo snapshot visivel somente
  na transacao seguinte. O contrato focado passou e fecha o recheck local.
- Harness temporal: gates amplos anteriores terminaram `587/588`, `586/588` e
  `587/588`; nenhum timeout foi aceito. `7f396b0` remove o polling de copia de
  128 MiB do conversor, separa instancias SAM no teste concorrente, ordena o
  teardown e usa watchdogs medidos. Conversor passou 20/20, SAM 20/20 suites,
  Details 10/10 e o CTest final passou `588/588` em `78.30 s`.
- Security ampla do gate de versao: Semgrep 508 arquivos, 24 regras e zero
  finding; Gitleaks 1.42 GB e zero leak; TruffleHog Git 9,090 chunks e os cinco
  arquivos alterados sem segredo. O scan Git emitiu warning de cleanup porque
  `ps` e bloqueado pelo sandbox, sem falha do scan de conteudo.
- Sol xhigh encontrou 1 P1 de recovery, 1 P2 de snapshot e 1 P2 de cobertura
  do cleanup. Os tres foram reproduzidos ou confirmados e corrigidos. O Sol
  confirmou os dois fixes de codigo; o recheck final apos o teste de cleanup
  excedeu a janela e nao recebeu credito. Clawpatch continuou invalido porque
  seu Codex CLI interno e antigo para `gpt-5.6-sol`.

Proxima atividade unica: fechar `TEST-DETERMINISM-DELTA` por suite, removendo
somente sleeps e polling com condicao observavel equivalente. Depois, executar
o benchmark formal de timestamp em filename. Esses dois slices locais sao os
que movem a divida nova de `78.6%` para `92.9%`.

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

1. Produzir profiling valido do prefetch; `xctrace` nao oferece `Time Profiler`
   nesta instalacao e `CPU Counters` falhou com `DTServiceHub`/politica do
   kernel.
2. Canonizar `clang-tidy` macOS com sysroot e reproduzir IDs Semgrep antes de
   dividir fixtures.
3. Revalidar handles, PowerShell, packaging e URL UNC em plataformas reais.

Detalhes, criterios e itens de prioridade menor ficam em
`RECOVERY_BACKLOG.md`. O plano executavel fica em
`docs/plans/2026-07-17-v0.9.10-audit-handoff.md`. O plano anterior
`docs/plans/2026-07-17-v0.9.10-follow-up.md` permanece como historico da
preparacao da release.

## Remotes e publicacao

| Remote | Provedor | Funcao | Estado confirmado |
| --- | --- | --- | --- |
| `origin` | GitLab | Repositorio e CI primarios | push de `b2369ac` bloqueado por OAuth `invalid_grant`; local 46 commits a frente |
| `bitbucket` | Bitbucket | Mirror obrigatorio | `b2369ac` confirmado em `master`; divergencia `0/0`; tag `v0.9.10` preservada |
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
