# Release v0.9.11

## Foco

Esta versao concentra hardening de importacao e SQLite, eliminacao de waits de
teste que nao provavam fase real e navegacao de derivadas previsivel. Nao ha
migracao de schema nesta versao.

## Entregas

- Recovery de consolidacao staged e journalizado ocorre antes do cleanup do
  rescan, preservando fontes owned apos crash entre commit e move.
- Contratos de importacao, conversao, crash e derivadas usam checkpoints
  causais em vez de polling temporal; cancelamento e reuso SQLite foram
  exercitados em fases reais.
- A selecao da tabela fixa a cadeia de derivadas. Navegar por pai, filha ou
  relacao troca os detalhes sem reconstruir a cadeia ou a raiz do grafo.
- Erros concorrentes de lookup e filhas diretas nao se apagam. O QML prioriza
  diagnostico sobre loading e preserva o indice quando uma SSA aparece duas
  vezes na cadeia.
- GUI e CLI recebem `0.9.11` de `PROJECT_VERSION` no build canonico.

## Validacao de release

- CMake `dev` reconfigurou e o build canonico passou. O warning de miniz e
  preexistente na dependencia cacheada e nao pertence a este release.
- CTest sequencial completo passou `602/602` em `64.93 s`, incluindo checks de
  versao GUI/CLI, importacao, SQLite, derivadas, QML, memoria e benchmarks.
- Semgrep amplo passou `24` regras em `512` arquivos; Gitleaks escaneou
  `1.43 GB`; TruffleHog nao encontrou segredo verificado ou desconhecido.
- Os hooks dos commits passaram formatacao e scanners staged. A tag anotada
  `v0.9.11` marca este HEAD validado.

## Limites conhecidos

- A sincronizacao completa de derivadas com procedencia, closure e resumo
  continua pendente de decisoes de produto sobre prioridade de fonte,
  multiparent, ciclos, orfaos e desativacao. O modelo C++ atual e direto e
  escalar.
- Validacao real de Windows/UNC, handles, PowerShell e packaging continua
  externa. Nao e declarada como concluida por esta nota.
