# Changelog

Mudancas relevantes deste projeto sao registradas neste arquivo.

## 0.9.17 - 2026-08-04

### Corrigido

- O report de executadas volta a publicar a semana em formato compacto
  `YYYYWW` apos a mudanca de `bucket_key` analitico (ex.: `202503`), sem
  truncar para o ano.
- Import incremental isola workbook rejeitado sem descartar irmaos ja
  aceitos no mesmo lote; falhas operacionais e avisos de derivadas ficam
  classificados no resumo do operador.
- Analytics: chaves ISO compactas, controles de periodo alcancaveis,
  exportacao de grafico com titulo/legenda e rotulos sem sobreposicao.
- Harness nativo Windows/WSL/Debian isolado por host; package Windows amd64
  MSVC gera zip, portable, installer e standalone.

### Validacao

- Windows release amd64/msvc: `645/645` testes, package em
  `dist/windows/amd64/msvc/final/`.
- Tag anotada `v0.9.17` publicada em GitLab (`origin`) e Bitbucket.

## 0.9.16 - 2026-07-25

### Operacao

- Tag anotada `v0.9.16` como marco antes do wizard unificado
  "Configurar Dados" e do hardening de first-run/import.

## 0.9.15 - 2026-07-20

### Corrigido

- O dialogo de importacao e rescan agora termina com titulo humano, resumo
  unico e inventario causal por arquivo, sem repetir o payload tecnico.
- O grafo de derivadas usa bounds reais, centra o conjunto no viewport e
  roteia ligacoes da segunda fileira sem atravessar os nos da primeira.
- O smoke macOS padrao nao depende mais de janela interativa, remove evidencia
  antiga antes dos gates, exige banco e preferencias novos e falha se a
  captura nao terminar ou nao produzir arquivo.
- Todas as fronteiras relevantes do smoke propagam falha explicitamente, sem
  depender da semantica fragil de `set -e` dentro de funcoes condicionais.

### Validacao

- `./run-macos-smoke-clean`: exit zero, `642/642` testes em `81.94 s`.
- PNG novo `1580x940`, 236228 bytes; banco runtime igual ao banco fonte.
- Semgrep, Gitleaks, TruffleHog, detect-secrets, formatadores, ShellCheck e
  clang-tidy sem finding novo bloqueante.

### Operacao

- `./run-macos-smoke-clean` e deterministico/offscreen por default.
- `./run-macos-smoke-clean --open` adiciona a janela interativa depois do
  preflight; seu tempo de vida depende do operador.

## 0.9.9 - 2026-07-15

### Corrigido

- Fontes substituidas durante o staging agora sao rejeitadas mesmo quando
  preservam tamanho e mtime; a identidade do arquivo tambem e comparada.
- Estados excepcionais com descricao (`ASE - ...`, `ADI - ...`, `SCC - ...`)
  continuam elegiveis por semana de cadastro sem exigir uma data inexistente.
- Estados terminais preservam e enriquecem indicadores de execucao, incluindo
  semana, responsaveis, execucao simples e descricao da execucao.
- Quando uma planilha traz `emission_datetime` e `issue_datetime`, emissao e a
  fonte primaria e issue e fallback somente quando emissao esta vazia.

### Validacao

- Build canonico `SSA_CPP_PRESET=dev ./scripts/build-macos.sh` concluido.
- Suite canonica local: 396 de 396 testes.
- Icone presente no bundle macOS e no ZIP/DMG gerados localmente.
- Backup local de `CMakeLists.txt` criado antes da alteracao de versao.

## 0.9.8 - 2026-07-15

### Adicionado

- Politica de snapshot SSA com nome da planilha como evidencia primaria,
  suporte auxiliar a data de criacao do arquivo e semana de cadastro para
  estados excepcionais.
- Perfis de fonte para executadas, derivadas/relacionadas, desvios e planilhas
  gerais, sem alterar o schema SQLite.
- Politica autocontida para reservar colunas futuras de historico de execucao;
  no schema atual, `descricao_execucao` continua sobrescrevendo o valor.
- Inspector opcional somente leitura usando Apache Arrow, desativado no build
  padrao e sem qualquer escrita no banco.

### Alterado

- Merge incremental preserva campos ricos quando a planilha nova e esparsa e
  permite enriquecer indicadores de execucao, atraso e parciais em snapshots
  elegiveis.
- `STE` e `SCA` permanecem terminais; `SCS` e transitorio. Um estado terminal
  nao e substituido por outro estado.
- Valores numericos invalidos sao rejeitados antes da mutacao SQLite, com
  rollback transacional.

### Validacao

- Build canonico `SSA_CPP_PRESET=dev ./scripts/build-macos.sh` concluido.
- Suite canonica local: 379 de 379 testes.
- Preset opt-in `dev-arrow` configurado com Apache Arrow 25.0.0 e smoke
  somente leitura concluido.

## 0.9.7 - 2026-07-15

### Adicionado

- Importacao explicita de derivadas em CSV, TXT, TSV, XLSX e XLSM, com XLS
  legado somente por selecao e preflight visivel do LibreOffice.
- Manifestos SAM validados antes do staging; workbook, schema e contagem fisica
  conferidos dentro de uma unica transacao SQLite atomica.

### Alterado

- Resultado SAM no limite de 200 linhas e rejeitado como potencialmente
  truncado; falha de qualquer setor impede o commit do lote completo.
- A ordem prioritaria de valores de setor e responsavel possui uma unica
  constante de dominio compartilhada entre display e SQL.
- O seletor de colunas recebe o acionador real, usa `Overlay.overlay` e resolve
  posicao e tamanho juntos com clamp nas bordas durante resize.

### Corrigido

- Importacao de derivadas rejeita self-loop, multiparent e filho inexistente,
  deduplica arestas e preserva pais ausentes em lote parcial.
- Fechar o menu de contexto nao fecha o seletor de colunas recem-aberto; o
  popup continua ancorado ao acionador durante redimensionamento da janela.

### Validacao

- Suite canonica local: 366 de 366 testes.
- Smoke QML prova clique real do menu, fechamento, ancoragem e resize visivel
  em 1180x760 e 1500x900.
- Scanners de codigo e segredos passaram; revisao independente final limpa.

## 0.9.6 - 2026-07-15

### Adicionado

- Contrato publico de importacao com `NoChanges`, resultado por arquivo e
  metricas reconciliadas de descoberta, validacao, escrita e consolidacao.
- Catalogo SSA com 77 campos canonicos, 184 labels de origem e 168 aliases
  normalizados, leitura de todas as worksheets, datas Excel 1900/1904 e
  rejeicao de formulas sem valor em cache.
- Journal SQLite transacional para retomada idempotente da consolidacao
  pos-commit.

### Alterado

- Rescans processam o acervo sequencialmente sem limite global de 64 arquivos;
  o guard de 64 permanece apenas para selecao externa explicita.
- O import SSA principal aceita somente XLSX. O conversor LibreOffice continua
  isolado e nao participa de discovery, rescan ou importacao SSA.
- Incremental usa merge seletivo e full rescan e all-or-nothing, sem transformar
  campo ausente em `NULL` nem publicar subconjunto valido.

### Corrigido

- Snapshot antigo nao substitui registro novo; empate temporal preserva estado
  e permite apenas enriquecimento autorizado.
- Full misto, header desconhecido, SSA invalida e conflito preservam o banco e
  as fontes anteriores.
- Shutdown forcado publica `Drained` somente sem start pendente nem arvore
  ativa; falha de termination bloqueia novos processos ate drain comprovado.
- Lider de processo que sai durante startup nao deixa descendente fora do
  registry ou da barreira.

### Validacao

- Suite canonica local: 340 de 340 testes.
- Fixture de 250 mil linhas respeita o limite adicional de RSS de 256 MiB.
- Testes de processo e cleanup foram repetidos 20 vezes sem corrida.

## 0.9.5 - 2026-07-14

### Corrigido

- Full rescan invalido preserva o banco anterior e nao consolida fontes sem
  commit SQLite valido.
- Cancelamento e falha removem somente artefatos de staging pertencentes a
  operacao; falha real de cleanup possui estado explicito e nao e inferida pelo
  instante do stop token.
- Falhas de preflight e leitura de diretorios sao `Failed` com razao publica
  segura e diagnostico tecnico separado; rejeicoes de politica continuam
  `Rejected`.
- Contagens de falha de consolidacao usam fontes nos retornos de preflight e o
  total de falhas inclui conversoes XLS malsucedidas.
- Override do conversor XLS exige arquivo executavel e os testes nao dependem
  da presenca de LibreOffice no `PATH`.

### Validacao

- Reutilizacao do supervisor apos barreira de shutdown bem-sucedida possui
  teste de regressao repetido, sem liberar a protecao fail-closed apos timeout.
- Markdownlint aceita headings iguais em releases diferentes e continua
  rejeitando duplicatas sob o mesmo pai.
- Build, testes e pacote macOS usam exclusivamente os presets e scripts
  canonicos documentados no repositorio.

## 0.9.4 - 2026-07-14

### Corrigido

- Conversao XLS indisponivel agora retorna mensagem publica segura e mantem o
  detalhe tecnico separado no diagnostico.
- Teste de retry da limpeza SAM reconhece que falha por permissao nao pode ser
  simulada quando o runner Linux executa como root.

### Validacao

- O fluxo macOS explicita o wrapper interativo `./run-macos-smoke-clean`, o
  smoke offscreen nao interativo e o empacotamento release.

## 0.9.3 - 2026-07-13

### Adicionado

- Estados publicos `Idle`, `Running` e `Canceling`, com terminal `Canceled`
  emitido somente depois que o trabalho realmente termina.
- Cancelamento contextual no status e encerramento responsivo com confirmacao
  forcada apos 10 segundos.
- Supervisor unico de arvores de processos com process group em Unix/macOS e
  Job Object em Windows.

### Alterado

- Consultas, valores distintos, exportacao, preferencias, presets, validacao
  de banco e workflows observam stop sem bloquear a thread da GUI.
- Mensagens publicas sao seguras; detalhes tecnicos ficam somente no log.
- Versao do projeto, GUI, CLI e pacotes atualizada para 0.9.3.

### Corrigido

- Cancelamento nao publica terminal antecipado, duplicado ou stale.
- Latest-wins cancela consultas distinct obsoletas e mantem apenas a ultima
  intencao pendente, sem deixar popup em loading permanente.
- Escritas SQLite abrem transacao antes de DDL e mutacoes persistentes;
  rollback e verificado e manutencao opcional pos-commit nao altera o terminal.
- Copia, conversao XLS e extracao XLSX removem temporarios parciais em falha
  ou cancelamento.
- Encerramento forcado impede novos starts, mata arvores registradas e nao
  mascara falha de cleanup como sucesso.

### Seguranca

- Cancelamento antes do commit preserva o estado anterior; morte de processo
  durante escrita deixa o banco integral, nunca parcialmente publicado.
- Arquivos JSON sao publicados por `QSaveFile` e artefatos por rename atomico.

## 0.9.2 - 2026-07-13

### Adicionado

- Historico de ate 10 estados de filtros na GUI, retorno de varios niveis e
  copia textual para a area de transferencia.
- Dialogos Ajuda e Sobre com contrato C++ real e versao de `PROJECT_VERSION`.
- Troca assincrona para outro banco SQLite validado em modo somente leitura.
- Consolidacao pos-commit dos arquivos efetivamente importados.
- Atualizacao SAM REST opcional por meio do projeto `scrap_report` local.

### Alterado

- Gates locais da 0.9.2 agora usam cppcheck com `compile_commands.json` e
  filtros de producao, `all_qmllint`, comparacao nao mutante do qmlformat e
  scans de segredo com escopo explicito.
- Preferencias passam ao schema 13 com migracao automatica de documentos no
  schema 12 para defaults SAM desabilitados.
- Versao do projeto, GUI, CLI e pacotes atualizada para 0.9.2.

### Corrigido

- Limpar a busca geral nao dispara mais duas consultas consecutivas.
- Undo de filtros preserva o contrato latest-wins e restaura somente
  condicoes, sem manter resultados antigos.

### Seguranca

- Banco alternativo e validado em modo somente leitura antes do processo de
  substituicao.
- Atualizacao SAM exige HTTPS, CA explicita e manifesto estrito, executa `uv`
  sem shell e nao aceita nem persiste senha, token ou segredo.
