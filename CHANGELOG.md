# Changelog

Mudancas relevantes deste projeto sao registradas neste arquivo.

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
