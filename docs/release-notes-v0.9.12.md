# Release v0.9.12

## Foco

Esta versao fecha a propagacao dos parametros de execucao de importacao ate
os fluxos de derivadas e SAM, com busy wait SQLite validado na fronteira de
cada operacao. Nenhuma tabela SQLite ou regra de negocio foi alterada.

## Entregas

- O refresh SAM preserva `sqlite_busy_wait_ms` desde as preferencias ate o
  `SamImportRequest` que abre a escrita SQLite.
- O preflight SAM rejeita parametros fora do contrato antes de iniciar o
  processo externo.
- O teste de preferencias de importacao espera o estado terminal antes de
  iniciar o rescan seguinte; a corrida de single-flight deixou de produzir
  falso negativo.
- O fluxo externo continua processando selecoes em blocos de ate 64 arquivos,
  sem rejeitar a selecao completa.

## Validacao

- Review final: clang-format, cppcheck, Semgrep C/security e
  detect-secrets sem findings.
- Testes afetados `ssa_qt_workflow_runner_tests` e
  `ssa_qt_sam_refresh_tests`: `2/2` em `7.57 s`.
- Build release e CTest release passaram pelo script canonico. A etapa de
  DMG foi bloqueada pelo host (`hdiutil: Dispositivo nao configurado`), e os
  artefatos temporarios foram removidos pelo cleanup do script; nao ha ZIP ou
  DMG 0.9.12 declarado como entregue.
- Commit `e2b9c28` e tag anotada `v0.9.12` foram publicados em `origin` e
  `bitbucket`; os refs de branch e tag apontam para o mesmo commit.

## Limites conhecidos

- `rows_per_chunk` particiona o leitor XLSX externo; o merger de derivadas
  preserva estado por fonte e nao declara chunking de arestas.
- Validacao real em Windows/UNC, handles, PowerShell e packaging externo
  continua pendente.
