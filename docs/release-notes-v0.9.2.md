# SSA Consulta Rapida 0.9.2

## Resumo

A 0.9.2 fecha lacunas pontuais de paridade com a GUI Python sem portar sua
arquitetura. A release adiciona historico simples de filtros, Ajuda e Sobre,
troca segura de banco, consolidacao de entrada e atualizacao SAM REST opcional.

Nao ha mudanca no contrato SQL de busca. Preferencias no schema 12 sao migradas
automaticamente para o schema 13, sem acao manual.

## Destaques

### Historico de filtros

- Guarda somente busca, filtros por coluna, avancados e exclusoes aplicadas.
- Mantem no maximo 10 estados aplicados anteriores. Reaplicar o estado atual
  identico nao cria entrada; estados antigos nao consecutivos podem se repetir.
- Permite desfazer um ou varios niveis e copiar a lista textual.
- Restaura a pagina 1, salva preferencias e executa uma unica consulta.
- Nao armazena linhas ou resultados filtrados.

### Ajuda e Sobre

- Ajuda documenta os operadores e filtros realmente implementados em C++.
- Sobre mostra produto, autor e a versao 0.9.2 derivada de
  `PROJECT_VERSION`.
- O Guia de instalacao existente foi preservado.
- Os dialogos nao acessam rede, subprocesso, banco ou Git.

### Outro banco pela GUI

- A selecao usa o dialogo nativo de arquivo.
- A validacao assincrona abre o SQLite somente para leitura e exige arquivo
  regular, `quick_check` valido, `ssa_table`, schema compativel e pelo menos um
  registro.
- Uma nova instancia inicia com `--db`; a atual encerra somente depois que a
  substituta iniciou.
- Falha de validacao ou startup mantem a sessao atual intacta.

### Consolidacao de entrada

- O manifesto de fontes nasce somente depois do commit SQLite.
- Arquivos com linhas validas seguem para `processadas/`.
- Arquivos sem linhas validas seguem para `processadas/nosurvivor/`.
- Arquivos desconhecidos, pendentes, sem proveniencia ou com falha permanecem
  no diretorio de entrada.
- Destinos existentes nao sao sobrescritos; o nome e tornado unico e o rename
  nominal e atomico.
- Falha ou cancelamento pos-commit permanece visivel como warning.

### Atualizacao SAM REST

- Desabilitada por padrao, com execucao manual ou timer persistido.
- Usa `uv` e o projeto `scrap_report` local sem shell.
- Exige projeto completo, CA regular nao vazia, URL HTTPS sem credenciais,
  setores validos e escopo `consulta`.
- Solicita perfil `panorama`, quatro anos, detalhes e ate 200 registros por
  setor.
- Todos os setores devem produzir manifestos estritos e XLSX novos. Lote
  parcial nao e importado.
- Um lote completo e importado uma unica vez e a GUI recarrega somente depois
  do commit.
- Existe um single-flight por instancia e cancelamento durante o shutdown.
- Nenhuma senha, token ou segredo e aceito ou persistido.

## Compatibilidade

- O arquivo de preferencias passa ao schema 13. Documentos no schema 12 sao
  migrados com o objeto `sam_refresh` desabilitado por default.
- O historico de undo e volatil e nao altera o schema.
- A troca de banco inicia outra instancia em vez de mudar o repository ativo,
  evitando consultas e importacoes em bancos diferentes.

## Limitacoes conhecidas

- A consulta SAM usa limite de 200 registros por setor. Paginacao fica para um
  ciclo posterior.
- Single-flight e por instancia; duas instancias podem atualizar ao mesmo
  tempo.
- A consolidacao tem risco TOCTOU teorico se um escritor local adversarial
  trocar um diretorio entre a verificacao de symlink e o rename por pathname.
  O threat model da 0.9.2 considera o workspace local confiavel.
- XPath/Playwright, cofre multiplataforma e escopos com credenciais nao fazem
  parte desta release.

## Validacao da release

Antes da tag, a release exige formatadores e analisadores C++/QML, scans de
segredo, build e testes dev, screenshots offscreen dos novos dialogos e menus,
e empacotamento macOS pelo script canonico. Resultados e contagens finais devem
ser registrados no report de entrega, sem inferir sucesso de servicos externos.

## Publicacao

A tag anotada `v0.9.2` deve apontar para o commit documental final e ser
publicada em `origin` e `bitbucket`. O mirror `gh` permanece fora da publicacao
enquanto a conta GitHub estiver suspensa.
