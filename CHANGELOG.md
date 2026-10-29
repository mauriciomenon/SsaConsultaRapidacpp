# Changelog

Mudancas relevantes deste projeto sao registradas neste arquivo.

## Nao publicado

### Adicionado

- Janela de progresso da importacao: botao "..." no canto do titulo com "Copiar
  relatorio", "Salvar relatorio (.txt)" e "Abrir arquivo de log".

### Alterado

- Importacao e reescaneamento: o relatorio lista cada arquivo com status (`[OK]`,
  `[SEM ALTERACAO]`, `[IGNORADO]`, `[REJEITADO]`, `[FALHOU]`, `[CANCELADO]`,
  `[SEM LINHAS VALIDAS]`), data de modificacao, tamanho, duracao, linhas invalidas e
  conflitos, e termina com um resumo de todos os contadores e a duracao total.
- O log `logs/ssa.log` (pasta de configuracao) passa a ser um diario: inicio, cada
  arquivo, resumo e fim de cada operacao ficam registrados em nivel Info. A lista de
  log da Ajuda continua so com avisos e erros.
- Repeticoes de SSA descartadas (primeira linha mantida) e o diagnostico de uma
  operacao concluida sem avisos deixam de aparecer como aviso; ficam no diario.

### Corrigido

- Importacao: a precedencia entre snapshots usa a data de modificacao do arquivo antes
  da data de criacao, como no Python. A copia de staging tem data de criacao nova e
  fazia um relatorio antigo vencer um mais recente.
- Importacao: datas em `dd/mm/aaaa`, `aaaa-mm-dd` e `aaaa-mm-dd 00:00:00` sao a mesma
  data; antes a mesma data em formatos diferentes contava como atualizacao ou conflito.
  Valores antigos gravados com hora `00:00:00` ou com `T` sao regravados uma vez na
  proxima importacao que tocar a SSA.
- Importacao: a copia da planilha para `docs_entrada` preserva a data de modificacao
  original, como no Python. Antes o reescaneamento via a data da copia, no relatorio e
  como data de reserva do snapshot.
- Lote revertido: os arquivos que nao causaram o erro aparecem como "lote revertido por
  erro em outro arquivo", e nao mais como falha operacional.

## 0.9.20 - 2026-10-26

### Corrigido

- Pacote Windows arm64: o build de release compila de novo os testes de importacao
  (MSVC arm64 pedia `/bigobj`).
- As tags 0.9.18 e 0.9.19 ficaram sem pacotes publicados; use 0.9.20, que tem as mesmas
  mudancas para quem usa o app.

## 0.9.19 - 2026-10-26

### Corrigido

- Pacotes: o release no GitHub volta a gerar macOS (arm64 e Intel), Windows (amd64 e
  arm64) e Debian (amd64 e arm64), com SHA256SUMS. O pacote Debian deixou de usar uma
  opcao do `dpkg-shlibdeps` que o dpkg do Ubuntu 24.04 nao tem.
- A tag 0.9.18 ficou sem pacotes publicados; use 0.9.19, que tem as mesmas mudancas.

## 0.9.18 - 2026-10-26

### Adicionado

- Filtros avancados: card "Derivacao" para ver todas as SSAs, so as raiz ou so
  as derivadas. O filtro ja existia na consulta, mas nao tinha controle na tela.
- Card "Derivacao": opcoes "Possui derivadas" (SSAs que tem ao menos uma derivada)
  e "Derivadas encerradas" (SSAs cujas derivadas estao todas em SCA, SCC, SES ou
  STE), como na GUI Python.

### Corrigido

- Pacote macOS: o script usa o macdeployqt da mesma Qt do build. Antes podia
  pegar o do Homebrew, de outra versao, e o app empacotado nao abria; agora o
  script falha se o pacote sair sem `qt.conf`.
- Busca: `-termo` exclui, como `!termo` e como na GUI Python. Um `-` sozinho
  continua buscando o proprio hifen.
- macOS: o app passa a levar o proprio SQLite (o do Homebrew, dentro do `.app`) em
  vez de usar o do sistema, que falhava ao comecar uma escrita enquanto outra parte
  do app lia o banco, inclusive na validacao de um banco escolhido e no backup.
- O botao redondo de tema, ao lado de "Preferencias", abre a janela "Personalizar
  tema", como o botao "Tema" da GUI Python. Antes um clique trocava o tema e o
  salvava na hora, sem confirmacao.
- macOS: a tabela podia abrir vazia com "Falha ao iniciar a operacao no banco de
  dados" logo apos iniciar o app, quando os filtros avancados carregavam ao mesmo
  tempo. O SQLite do sistema falhava ao comecar uma escrita enquanto outras
  leituras abriam o banco; as leituras agora abrem o banco sem permitir escrita
  por SQL, de um jeito que o SQLite do sistema aceita.
- Pacote Windows: deixou de levar `data\ssas.db`. Com o banco ao lado do
  executavel, o portable gravava importacoes, rescans e preferencias numa pasta
  temporaria apagada ao fechar, e o instalador gravava em `Program Files`. Agora
  o app usa `%USERPROFILE%\.ssaconsultarapida`, como no macOS e no Debian.
- Build Windows: `scripts/build-windows.ps1` abre o ambiente do Visual Studio
  mesmo quando ja ha um `cl.exe` no PATH (antes o link falhava com
  `LNK1104 kernel32.lib`) e, em Windows ARM64, usa o compilador ARM64 nativo em
  vez do x64 emulado.
- Pacote Windows ARM64: `scripts/package-windows.ps1 -Arch arm64` gera o
  pacote; antes ele sempre parava exigindo `VCRUNTIME140_1.dll`, uma DLL que
  so o executavel x64 usa.
- Rescan: a copia completa do banco usada durante o rescan fica ao lado do
  banco e, se um rescan for interrompido, o seguinte a remove; antes ela ficava
  para sempre na pasta temporaria do sistema.
- Configuracao inicial: pastas temporarias deixadas por uma configuracao
  interrompida sao removidas na proxima tentativa. Importar ate 64 planilhas
  com o banco interrompendo a gravacao mostra o resumo e a causa, como em lotes
  maiores. Uma pasta comum com nome de copia temporaria em `docs_entrada` nao
  bloqueia mais os rescans.
- Listas suspensas perto do rodape da janela (por exemplo nos cards de
  filtros avancados da ultima linha) abrem para cima, ancoradas no campo; antes
  abriam uma faixa de uma linha abaixo dele.
- Configuracao inicial de dados com planilhas: refazer a configuracao com as
  planilhas ja em `docs_entrada/processadas` (banco apagado) funciona; a copia
  nova recebe o proximo nome livre (`nome__1.xlsx`), como na importacao. Uma
  `docs_entrada` em outro disco ou unidade de rede tambem funciona.
- Busca e filtros de texto ignoram maiusculas e minusculas tambem em letras
  acentuadas, como a GUI Python: `MANUTENCAO`, `manutencao` e `Manutencao`
  escritos com cedilha e til encontram as mesmas SSAs (no banco de teste, 529
  nas tres; antes 67 ou 462 conforme a caixa digitada).
- Rescan: uma planilha de `docs_entrada` substituida durante o rescan (por
  exemplo pelo OneDrive ou por uma exportacao com nome fixo) faz o rescan
  falhar sem publicar nada e continua em `docs_entrada`; antes a versao nova
  ia para `processadas` sem ter sido importada.
- Configuracao inicial: uma pasta `docs_entrada/processadas` que seja atalho
  (symlink) e recusada com mensagem clara; antes as planilhas eram copiadas
  para fora do projeto.
- Grade: cancelar ou falhar a troca do tamanho de pagina volta tambem o
  tamanho exibido; antes o paginador mostrava, por exemplo, "pagina 10 de 1".
- Barra de status: uma importacao que terminou sem mudancas nao e mais
  substituida pela falha de uma importacao anterior cujo recarregamento ainda
  estava em curso.
- Seletor de colunas e Preferencias: o filtro digitado num deles e fechado em
  seguida nao filtra mais a lista do outro.
- Rescan e importacao depois de uma queda no meio de uma gravacao: o banco com
  journal pendente e recuperado na primeira operacao (volta ao ultimo estado
  gravado); antes `--rescan` falhava sempre com `import_consolidation_resume_failed`
  ate a GUI abrir o banco.
- Importar arquivos (mais de 64 planilhas): quando o banco interrompe um lote
  (por exemplo disco cheio), o resultado mostra as SSAs dos lotes ja gravados,
  a grade recarrega e o diagnostico traz a causa; antes vinha uma falha
  generica sem resumo e com "no such savepoint" no lugar da causa.
- Rescan: a copia temporaria `.part` deixada por uma importacao interrompida
  (queda ou encerramento forcado) e removida no rescan seguinte e nao conta mais
  como arquivo pendente; a de outro banco que use a mesma pasta fica intacta.
- Importar arquivos: uma planilha ignorada (cabecalho nao reconhecido) nao
  deixa mais a copia temporaria `.ssa-staged-...` em `docs_entrada`.
- Filtros: clicar num atalho de situacao, remover um filtro ou trocar o setor
  rapido descarta as listas de valores ja carregadas uma vez so (antes 3 a 4
  vezes, cada uma com nova consulta de setores); digitar no editor de coluna,
  focar uma coluna ou trocar o operador de um card nao descarta mais nada.
- Janela de detalhes destacada: clicar no grafo numa SSA e voltar para a SSA ja
  exibida antes de a outra carregar mantem a SSA exibida; antes a carga
  pendente terminava e a substituia.
- Atalhos de situacao: com um filtro digitado na coluna Situacao (por exemplo
  `AP`, que busca "contem", ou `^A`), clicar num atalho ou em "Limpar atalhos"
  nao troca mais esse termo por igualdade exata; o atalho so acrescenta ou
  remove o proprio codigo. Antes `AP` virava `=AP` e as SSAs sumiam do
  resultado.
- Importacao: o resumo conta como ignoradas (`skipped`) as linhas de
  continuacao dos relatorios por evento (`Reprogramacao #2`, `Desvio #2` e
  seguintes, sem numero de SSA); antes elas sumiam da contagem.
- Configuracao inicial de dados com planilhas: se alguma planilha for
  rejeitada, a configuracao falha e informa qual, sem criar banco nem copiar
  arquivos para `docs_entrada`. Antes ela terminava como "concluida" com parte
  das SSAs e a planilha rejeitada sumia do resultado.
- Atualizacao pelo SAM: quando um processo cancelado ou expirado nao termina,
  nenhum novo processo e iniciado ate a aplicacao ser reiniciada, e a recusa
  informa o motivo. Antes um novo processo podia comecar ao lado do que ficou
  preso.
- Ajuda > Guia de instalacao abre o guia tambem na aplicacao instalada: os
  pacotes macOS, Windows, Debian, Arch e Linux passam a levar o `README.md`, e
  a aplicacao o procura na pasta do projeto e depois na do pacote. Antes so
  funcionava num clone de desenvolvimento.
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
  Textos que diferem apenas por acento (ex.: `AQUISICAO` com e sem cedilha e til) deixam
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
- Busca geral: deixa de procurar em 10 colunas tecnicas (parciais, situacao da
  parcial, execucao parcial, desvios, equipamentos retirado/instalado,
  desativacao, instalacao estimada, executado, concluido) e passa a procurar no
  nome da planilha de origem, como a versao Python; antes "APV" trazia SSAs cuja
  parcial estava APV.
- Campos numericos (intervalo do SAM, linhas por bloco, espera do SQLite,
  tamanho da pagina, largura de coluna) aceitam digitar o valor; antes so
  mudavam pelas setas, um passo por clique.
- Exportar resultados: o CSV segue exatamente a consulta mostrada na tabela; antes
  usava o texto digitado na busca geral mesmo sem aplicar, e o arquivo divergia
  da tela.
- Colunas: fechar Preferencias ou o seletor de colunas sem "Aplicar" descarta as
  mudancas de colunas pendentes; antes elas apareciam no outro editor e eram
  aplicadas no proximo "Aplicar".
- Importacao: quando uma consolidacao interrompida antes nao consegue terminar
  (arquivo pendente sumiu) ou e cancelada, o rescan e a importacao informam
  falha ou cancelamento; antes diziam "concluido com avisos" (CLI saia com 0)
  sem ter importado as planilhas novas. No SAM, uma falha parcial nessa etapa
  nao aparece mais como "cancelada".
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
  invalido quando uma celula termina com espaco nao separavel. Esse espaco e os
  demais espacos Unicode que a versao Python remove das bordas da celula (ex.:
  separador de linha U+2028, espaco ideografico U+3000) agora sao removidos
  inteiros.
- Analytics: os relatorios rapidos "Prazo das pendentes por area",
  "Executadas por setor (semana)" e "Executadas por setor (mes ISO)" voltam a
  gerar a analise; antes so configuravam os filtros e nao rodavam.
- Windows: nomes de planilha de derivadas com acento ou caracteres fora da
  pagina de codigo do sistema aparecem corretos no resumo e no progresso; antes
  saiam trocados ou faziam a importacao falhar.
- Analytics: abrir a aba "Analise customizada" enquanto o painel carrega nao
  deixa mais o painel vazio, e mudar o periodo do painel nao descarta mais as
  listas de divisao, setor e pessoa em carga; cada tipo de consulta so substitui
  a anterior do mesmo tipo. "Cancelar" interrompe todas as consultas em curso.
- Analytics: depois de uma importacao, a janela de analises recarrega o painel
  e as listas da analise customizada (na hora, se estiver aberta, ou ao reabrir)
  e mantem as divisoes, setores e pessoas selecionados que ainda existem; antes
  ficava vazia e perdia as selecoes. Fechar a janela durante uma carga e
  reabrir pede os dados de novo. Se um setor ou divisao selecionado deixou de
  existir, as listas dependentes sao pedidas de novo antes de remover pessoas, e
  um relatorio rapido iniciado enquanto o painel carrega roda quando ele termina.
- Filtro de coluna em datas (Data de cadastro, Prazo limite e as demais exibidas
  como dd/mm/aaaa) aceita o formato mostrado na grade: `21/12/2021`, `12/2021`
  e `!21/12/2021` voltam a encontrar ou excluir as SSAs, como na versao Python.
- Grade: cancelar (ou ver falhar) uma troca de pagina ou de ordenacao devolve o
  paginador e o indicador de ordenacao ao que a tabela mostra; antes eles ja
  mostravam o pedido novo, e a pagina seguinte pulava uma.
- Arquivo: "Carregar outro banco" e "Configurar dados..." ficam desabilitados
  durante importacao, rescan, manutencao ou exportacao; antes a troca relancava
  o app e cancelava a operacao em curso sem aviso.
- Exportacao: enquanto uma exportacao roda, "Exportar resultados" e "Exportar
  lista" ficam desabilitados, e um segundo pedido e recusado com aviso; antes era
  ignorado em silencio e o segundo arquivo nunca era gravado.
- macOS: o botao Aplicar da busca nao corta mais o simbolo de retorno, e o
  espaco entre a contagem de SSAs e "Importar XLSX" volta a ser exato.
- Filtros avancados: um filtro de texto importado sem prefixo (ex.: `APV,ADM` no
  JSON de filtros) aparece marcado no popup de valores, como a consulta o le
  (`=APV,=ADM`); antes nada vinha marcado e "Aplicar" apagava o filtro.
- Filtros avancados: o cartao Macro cresce ja durante "Carregando relatorio" e
  quando o relatorio falha, sem desenhar essas linhas por cima da fileira de
  baixo; no macro so de grafico, a mensagem de erro cortada mostra o texto
  inteiro ao passar o mouse.
- Colunas: o filtro por nome digitado no seletor de colunas nao passa mais para
  a lista de colunas das Preferencias (e vice-versa); antes as Preferencias
  abriam mostrando so parte das colunas com o campo de filtro vazio.
- Analytics: cancelar a primeira carga da janela de alerta nao deixa mais o
  painel parado; "Salvar alerta" volta a atualizar os graficos, e reabrir a
  janela recarrega o painel.
- Importacao: dois bancos usando a mesma pasta `docs_entrada` ("Trocar banco")
  nao apagam mais a copia temporaria de planilha externa que o outro banco ainda
  precisa consolidar; antes, voltar ao primeiro banco travava toda importacao
  com "consolidation source and destination state is ambiguous". As copias novas
  levam a marca do banco no nome; copias antigas, sem marca, seguem a regra
  anterior.
- Campos numericos: um valor digitado fora do limite ou que o programa arredonda
  (ex.: espera do SQLite em multiplos de 5) volta a mostrar o valor realmente
  usado; antes o campo continuava exibindo o numero digitado.
- Importacao: quando uma importacao falha ou e cancelada depois de gravar parte
  das linhas, a grade recarrega e a barra de status continua mostrando a falha;
  antes o recarregamento apagava o erro e mostrava "Consulta concluida".
- Filtros avancados: quando a lista de valores de reprogramacoes nao carrega
  (banco travado, erro), o popup mostra a falha e "Tentar novamente", como os
  filtros de texto; antes mostrava "Sem valores".
- Analytics: com a janela de analises aberta de um mes para o outro, o mes que
  acabou de fechar passa a ser alcancavel pelo ">" ao reabrir ou voltar a janela;
  antes so "Ultimo mes completo" chegava nele.
- Menu Exibir: o item "Ocultar detalhes" passa a se chamar "Mostrar detalhes",
  como a opcao das Preferencias; a marca indicava o contrario do rotulo.
- Rescan e assistente de dados: a copia do banco espera o tempo configurado
  (`--sqlite-busy-wait-ms`, padrao 3 s) quando outro programa esta gravando no
  banco de origem; antes falhava na hora com "Falha ao copiar banco".
- Detalhes: o painel da SSA selecionada na grade mostra as relacoes "Origem" e
  "Relacionada" mesmo com essas colunas ocultas (padrao); antes so a janela de
  detalhes destacada as mostrava.
- Inicializacao: uma falha ao abrir o app (ex.: arquivo de preferencias
  invalido) e o aviso de analises indisponiveis passam a ficar gravados em
  `config/logs/ssa.log` e na lista de log recente; antes o app fechava sem
  deixar registro quando nao havia console.

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
