# Release notes v0.9.15

## Entregas

- Dialogo de importacao/rescan termina corretamente, resume em linguagem
  humana e lista arquivo e motivo sem duplicar o log tecnico.
- Grafo de derivadas centra o conjunto e evita arestas atravessando nos.
- Smoke macOS padrao e offscreen, fail-closed e protegido por watchdog.
- Versao GUI/CLI atualizada para 0.9.15.

## Validacao local

- Commit funcional `9415847`; smoke/version `736edcc`.
- `./run-macos-smoke-clean`: `642/642`, `81.94 s`, exit zero.
- PNG 1580x940 novo e banco runtime identico ao banco fonte.
- Corpus real anterior: 1692 arquivos, 458864 linhas, integridade e FK verdes,
  `numero_desvios` somente integer/null.

## Limites

- Windows/UNC/SMB, PowerShell e packaging nao-macOS continuam externos.
- Profiling valido do prefetch continua bloqueado.
- A troca acidental de tema pelo seletor circular nao foi alterada nesta
  release; `ssa-dark` continua disponivel em Preferencias.
- Estado de publicacao deve ser confirmado por refs remotos; nao inferir pelo
  sucesso local.
