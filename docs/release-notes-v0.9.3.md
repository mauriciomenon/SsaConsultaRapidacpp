# SSA Consulta Rapida 0.9.3

## Resumo

A 0.9.3 e um patch de estabilizacao sobre a 0.9.2. Ela torna cancelamento,
atomicidade SQLite, processos externos e encerramento da GUI observaveis e
verdadeiros. A tag v0.9.2 permanece inalterada em `6bec8c0`.

## Cancelamento real

- Operacoes expoem `running`, `canceling`, `canCancel` e cancel idempotente.
- `Cancelando...` aparece durante a solicitacao. `Canceled` so e publicado
  depois do terminal do worker, SQLite e arvore de processos.
- Cancel repetido e no-op; nova operacao nao inicia durante `Canceling`.
- O botao no status cancela todas as atividades cancelaveis ativas.
- Mensagens da GUI sao seguras; excecoes, caminhos e mensagens SQLite brutas
  ficam somente no log.

## Filtros e concorrencia

- Undo, preferencias, reset e substituicao invalidam consultas distinct stale.
- A consulta obsoleta recebe stop; somente a ultima intencao pendente inicia e
  publica.
- Cada leitura SQLite usa conexao read-only por operacao e busy handler curto
  sensivel a stop.
- Se o filtro B falhar, B permanece nos controles para retry/undo e a tabela
  informa que ainda mostra o ultimo resultado bem-sucedido.

## SQLite e arquivos

- Transacoes comecam antes de DDL, delete e escrita persistente.
- Antes do commit, cancelamento implica rollback explicito e verificado.
- Depois do commit, a operacao permanece `Succeeded`; etapa opcional
  interrompida vira warning.
- Copia e conversao usam temporario exclusivo, observam stop em blocos e
  publicam por rename somente no sucesso. XLSX e extraido incrementalmente.
- Testes por subprocesso provam banco integral antes/depois de morte durante a
  escrita e executam `PRAGMA integrity_check` apos reabrir.

## Processos e shutdown

- Unix/macOS usam sessao e grupo de processo proprios. Windows usa Job Object
  com `KILL_ON_JOB_CLOSE`.
- Force-stop impede novos starts, cobre a janela `Starting`, mata a arvore e
  nunca converte falha de cleanup em sucesso.
- O primeiro close mantem o event loop responsivo enquanto solicita todos os
  cancelamentos. Depois de 10 segundos, novo close oferece encerramento
  forcado.
- Preferencias e presets usam `QSaveFile`; o destino nunca e substituido por
  conteudo parcial.

## Compatibilidade e validacao

- O schema de preferencias continua 13; nao ha migracao de dados nesta patch.
- macOS arm64 e validado localmente. Linux depende do pipeline externo.
  Windows possui implementacao e teste condicional, mas so sera declarado
  comprovado quando um runner Windows executar o gate.
- O unico item diferido novo e a medicao de cache para `roleNames()`.
  Ajuda/Sobre nao apresentou flakiness objetiva nesta rodada.

## Publicacao

A tag anotada `v0.9.3` deve ser criada apenas depois dos gates locais, pacote
macOS e verificacao dos refs em `origin` e `bitbucket`. O remote `gh` permanece
fora da publicacao.
