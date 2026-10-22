# Changelog

Mudancas relevantes deste projeto sao registradas neste arquivo.

## Nao publicado

### Corrigido

- Importacao: planilha com aba valida seguida de aba nao reconhecida e
  rejeitada inteira; antes suas linhas entravam no banco quando outro arquivo
  valido estava no mesmo lote. Conflito de duplicata no rescan completo agora
  informa o nome do arquivo.
- Busca: negacao (`!MOTOR` e exclusao por coluna) nao some mais com SSAs que
  tem campo vazio; filtro de semana sem ano volta a encontrar resultados.
- Analytics: SSAs cadastradas dentro do periodo deixam de aparecer como
  cadastradas antes do periodo nas series de eventos.
- CLI: comandos de manutencao (`--reset-db`, `--rescan` e outros) combinados
  com `--export` ou `--details` sao recusados antes de qualquer efeito; antes o
  reset rodava e a exportacao era ignorada. `--export` com `--details`,
  `--asc`/`--desc` sem `--sort` e `--page-size` menor que 1 tambem sao
  recusados. A exportacao CSV segue a mesma ordem da listagem, e valores com
  tabulacao ou quebra de linha nao quebram mais a saida tabular.
- Filtros avancados: reabrir valores com exclusao (`!=APV`) e aplicar nao
  corrompe mais o filtro; confirmar "Ate" nas semanas de emissao e execucao
  nao apaga mais o "De"; a semana atual usada como "Ate" automatico segue a
  semana ISO correta (a formula antiga errava a partir de 2027-01-04);
  reprogramacao "0" passa a filtrar; o botao "..." continua disponivel com
  inclusao e exclusao juntas; o atalho "SSAs para baixar" pode ser reaplicado
  depois de limpar os filtros.
- Analytics: os relatorios rapidos voltam a iniciar a analise depois de
  carregar as dimensoes.
- Importacao: conversao de `.xls` com ponto no nome (ex.: `relatorio.v1.xls`)
  passa a encontrar o arquivo convertido; planilha com referencia de texto
  compartilhado invalida e rejeitada em vez de importar a celula vazia.
- Encerramento de processos externos aguarda 5 s apos o kill forcado; antes
  aguardava 5 ms e podia acusar falha de parada sem motivo.
- Preferencias: colunas `equipamento`, `descricao_localizacao` e
  `grau_prioridade_*` escolhidas pelo usuario nao somem mais ao reiniciar; a
  atualizacao de preferencias salvas nas versoes 12 e 13 nao reaplica mais
  migracoes antigas sobre colunas e larguras personalizadas.
- Importacao: exportacoes do SAM no mesmo minuto passam a ser ordenadas pelo
  horario do arquivo (segundos), em vez de rejeitadas como conflito; no acervo
  real as planilhas rejeitadas cairam de 133 para 30. Cada conflito restante e
  registrado no log com SSA, campo, os dois valores e os dois arquivos.
  Textos que diferem apenas por acento (ex.: `AQUISICAO` e `AQUISIÇÃO`) deixam
  de ser conflito e a grafia acentuada e mantida (30 -> 26 no acervo real).
- Importacao: SSA repetida dentro da mesma planilha (o SAM exporta uma linha
  por execucao) mantem a primeira linha e registra a descartada no log; antes
  a planilha inteira era rejeitada, e "SSAs com Reprogramacoes" nunca
  atualizava o banco. Conflito entre planilhas diferentes do mesmo instante
  continua rejeitado.
- Importacao: o relatorio SAM "SSAs Legadas" (`SAAs_*.xlsx`) passa a ser
  ignorado como familia conhecida, como no Python; antes aparecia como
  planilha rejeitada por falta de colunas. A importacao dessas SSAs fica para
  uma versao futura.
- Importacao: o rotulo "Reprogramacao #N" do relatorio "SSAs com
  Reprogramacoes" (uma linha por reprogramacao) nao e mais gravado como numero
  de reprogramacoes; antes a contagem real vinda de outras planilhas era
  trocada por 1 (641 SSAs no acervo real). A contagem continua em "No de
  Reprogramacoes" e o total em "Total de Reprogramacoes", como no Python.
- Importacao: um rescan incremental que falha (por exemplo, com uma planilha
  corrompida no lote) nao lista mais as outras planilhas como aplicadas; nada
  e gravado nesse caso, e o resumo agora mostra todas como nao aplicadas.
- Importacao: as colunas "Desde" e "Ate" (`desde`, `desde_1`, `desde_2`, `ate`,
  `ate_1`, `ate_2`) passam a ser gravadas como data e hora ISO
  (`2026-02-06 14:55:07`), como no Python, e sao exibidas com a hora. Antes o
  texto `dd/mm/aaaa` era gravado como veio, misturando formatos na mesma coluna
  (ordenacao e filtro errados) e gerando conflito falso entre planilhas do
  mesmo instante. Valores antigos sao normalizados quando a SSA e reimportada.
- Importacao: nomes de arquivo com data ISO e hora (ex.: `SSA_2026-07-15T10.00`)
  passam a considerar a hora; antes duas exportacoes do mesmo dia valiam como
  o mesmo instante e podiam gerar conflito falso.
- Importacao: celula de data com numero serial alem de 31/12/9999 rejeita a
  planilha com "xlsx date serial out of supported range"; antes um valor muito
  grande tinha conversao indefinida.
- Tabela: navegar com as setas mantem a linha selecionada visivel; antes a
  selecao descia para fora da area visivel.
- Analytics: falha ao exportar grafico ou CSV (PNG, SVG, CSV, zeros em CSV)
  aparece no aviso de erro da janela; antes nada era mostrado.
- Historico de logs: mensagens longas no painel de detalhe passam a rolar; antes
  o final era cortado.
- Atualizacao SAM: cancelar ou encerrar um processo cujos filhos ainda estao
  ativos nao ocupa mais um nucleo inteiro da CPU durante a espera (ate 5 s).
- Detalhes: navegar para outra SSA relacionada e voltar antes de ela carregar
  nao troca mais a SSA exibida pela que estava carregando.
- Importacao: o rotulo "Desvio #N" do relatorio "SSAs com Desvio na
  Programacao" (uma linha por desvio) nao e mais gravado como numero de
  desvios; antes a contagem real ("Numero de Desvios") era trocada por 1 (180
  SSAs no acervo real). Texto legado "Desvio #N" no banco vira vazio quando a
  SSA e reimportada.
- CLI: falha ao escrever a listagem ou os detalhes na saida (disco cheio, erro
  de escrita) sai com codigo 1 e "error: cannot write output"; antes saia com 0.
- Analytics: trocar pessoas ou a ordenacao com uma analise customizada em
  andamento descarta o resultado antigo; antes o grafico da selecao anterior
  aparecia como se fosse o novo e o botao Analisar ficava bloqueado ate ele chegar.
- Importacao: selecionar de novo uma planilha depois de um fechamento inesperado
  no meio da consolidacao nao falha mais com "cannot inspect pending recovery
  source"; a recuperacao conclui a movimentacao pendente e a planilha e importada.
- Analytics: cancelar logo depois de salvar a janela de alerta (dias) nao mostra
  mais "cancelado" com o valor antigo; o valor ja gravado aparece como salvo.
- Importacao: quando a mesma selecao traz planilhas comuns e de derivadas e a de
  derivadas falha, o resumo passa a mostrar "Importacao parcial" com a falha;
  antes dizia "concluida com avisos ... nenhuma falha".
- "Arquivo > Carregar dados" le o banco de novo (pagina, total e opcoes de
  filtro) e volta para a primeira pagina; antes repetia a pagina guardada em
  memoria e nao mostrava alteracoes feitas no banco por outro programa.
- Filtros avancados: gerar um relatorio de SSAs executadas nao esvazia mais a
  lista de valores ja carregada nos filtros nem refaz a consulta de setores;
  antes cada relatorio descartava essas listas como se o filtro tivesse mudado.
- Filtros avancados: uma lista de valores aberta durante "Carregar dados" ou ao
  fim de uma importacao busca os valores de novo; antes ficava vazia ate ser
  fechada e reaberta.
- Log: falha ao pre-carregar as proximas paginas da tabela e falha do relatorio
  de SSAs executadas passam a ser registradas no log com o detalhe tecnico;
  antes a primeira sumia em silencio e a segunda so aparecia na tela.
- CLI: um rescan que falha e nao grava nada termina a linha de contadores com
  `committed=0`; antes os contadores da tentativa (`inserted=306`) pareciam
  linhas gravadas.
- Janela de progresso: as areas de saida e de avisos guardam as 4000 linhas mais
  recentes, como a versao Python, e mostram o texto sem interpretar marcacao;
  antes cresciam sem limite em importacoes grandes.
- Importacao: numa selecao com planilhas SSA e de derivadas, se a parte SSA
  falha ou e cancelada, a planilha de derivadas aparece no resumo como nao
  importada; antes sumia da lista sem explicacao.
- Tabela: uma importacao, rescan ou atualizacao SAM que termina sem gravar nada
  nao recarrega mais a tabela; antes voltava para a pagina 1, perdia a selecao e
  limpava as listas de valores dos filtros sem nenhum dado ter mudado.
- Filtros avancados: o relatorio "SSA Executadas" deixa de sair vazio depois do
  atalho "SSAs para baixar" ou com "Excluir SCA, SES, STE"; os filtros de
  situacao e o ano de execucao salvo nao restringem mais o relatorio, que define
  sozinho as SSAs executadas (STE/SES), como na versao Python.
- Importacao: uma importacao que leva mais que o tempo de espera do SQLite
  (padrao 3 s) volta a esperar a tabela ser liberada ao gravar; antes falhava na
  hora e desfazia o lote inteiro se a grade estivesse lendo o banco naquele
  instante. Vale tambem para rescan, manutencao e derivadas.
- macOS: a busca com letras acentuadas (ex.: "inspecoes" com c cedilha e o til)
  volta a encontrar resultados, e a importacao nao grava mais texto UTF-8
  invalido quando uma celula termina com espaco nao separavel; esse espaco agora
  e removido inteiro, como na versao Python.
- Analytics: os relatorios rapidos "Prazo das pendentes por area",
  "Executadas por setor (semana)" e "Executadas por setor (mes ISO)" voltam a
  gerar a analise; antes so configuravam os filtros e nao rodavam.
- Windows: nomes de planilha de derivadas com acento ou caracteres fora da
  pagina de codigo do sistema aparecem corretos no resumo e no progresso; antes
  saiam trocados ou faziam a importacao falhar.

### Alterado

- Resumo da importacao: SSAs que a planilha nova trouxe com os mesmos dados
  (so o arquivo de origem muda) aparecem como "verificadas", separadas das
  "atualizadas", que agora contam apenas mudanca real de valor. Ex. real:
  "SSAs com Reprogramacoes" do dia seguinte passa de 3.035 atualizadas para 16
  atualizadas e 3.019 verificadas. A linha de resumo da CLI ganha `verified=N`.
- Importacao que falha ou e cancelada depois de gravar linhas agora atualiza a
  tabela e o analytics, mantendo a mensagem de falha; se a recarga apos uma
  importacao bem-sucedida falhar, o erro da recarga continua visivel em vez de
  ser coberto pela mensagem de sucesso.
- Configuracao inicial com "Importar banco" usa a API de backup do SQLite: um
  banco aberto por outro aplicativo em modo WAL (como o SSA Python) e copiado
  com as transacoes ainda no arquivo `-wal`; antes a copia era recusada como
  banco invalido.
- Exportacao de grafico (PNG e SVG) so substitui o arquivo quando a gravacao
  termina por completo.
- Log rotativo: mensagens com quebra de linha ficam em uma unica linha (quebras
  escapadas como `\n`), e falha de gravacao no disco passa a ser detectada.
- Colunas: `numero_ssa` nao pode mais ser ocultada (ocultar quebrava detalhes,
  SAM e copia); preferencias antigas sem ela a recebem de volta na primeira
  posicao. Alteracoes de colunas nao aplicadas no seletor nao sao mais salvas
  ao trocar o tema ou fechar o aplicativo.

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
