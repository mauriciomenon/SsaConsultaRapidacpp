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
- Package release: `642/642` em `67.10 s`; ZIP integro; DMG UDZO/CRC32;
  assinatura deep/strict valida; somente driver SQLite no bundle.
- SHA-256 ZIP: `dae29615c7139d78e45a79498afcbd918599384cc02d1a9578320b2be83e6465`.
- SHA-256 DMG: `3131270ced9ed2e305c6849eaa89fd5d3161087f3b33a1384ea5b2464f75a580`.
- Corpus real anterior: 1692 arquivos, 458864 linhas, integridade e FK verdes,
  `numero_desvios` somente integer/null.

## Limites

- Windows/UNC/SMB, PowerShell e packaging nao-macOS continuam externos.
- Profiling valido do prefetch continua bloqueado.
- A troca acidental de tema pelo seletor circular nao foi alterada nesta
  release; `ssa-dark` continua disponivel em Preferencias.
- Estado de publicacao deve ser confirmado por refs remotos; nao inferir pelo
  sucesso local.
