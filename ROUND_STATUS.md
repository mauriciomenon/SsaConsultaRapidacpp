# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo
antes de interpretar sincronizacao Git, validacao local ou estado externo.

Ultima verificacao local: 2026-07-17

## Snapshot da preparacao v0.9.10

- Branch: `master`.
- Baseline funcional antes do commit documental: `d34f92d`.
- Commits finais da estabilizacao: `a75c0bc` e `d34f92d`.
- `origin/master` e `bitbucket/master` foram confirmados em `d34f92d` antes da
  preparacao da release.
- Versao preparada: `0.9.10`; tag esperada: `v0.9.10` anotada no commit desta
  atualizacao documental.
- Working tree tracked estava limpo antes desta atualizacao.
- Sete entradas locais untracked permanecem fora de staging e da release.
- Nenhuma branch, worktree, PR, merge ou publicacao em GitHub foi criada.

## Estado implementado

### Importacao, atomicidade e banco

- O rescan publica banco e journal duravel antes de mover fontes.
- Crash, cancelamento e consolidacao parcial permanecem retomaveis.
- A retomada do journal ocorre antes da importacao do lote corrente.
- Falha antes da publicacao preserva banco e fontes originais.
- Staging compara identidade, tamanho e mtime e classifica desaparecimento
  depois do snapshot inicial como fonte alterada.
- `SqliteSsaImportWriter` exige capability do workflow que possui o lock.
- Locks usam identidade canonica do banco e cobrem aliases por symlink.
- Schema SQLite nao mudou e continua usando chaves ASCII canonicas.
- Headers PT/ES/EN sao normalizados por `SsaSpreadsheetHeaderCatalog`.
- `numero_desvios` aceita inteiro ou `Desvio #<inteiro>`; ambiguidades falham.

### Camadas C++ e exportacao

- `domain::ColumnDef` contem apenas chave, tipo e regra de busca geral.
- Os 85 labels gerais ficam em `application::SsaColumnLabelCatalog`.
- Largura e visibilidade ficam em presentation.
- GUI e CLI enviam cabecalhos CSV explicitamente pelo request do port.
- Infra valida cardinalidade e usa chaves canonicas quando labels sao omitidos.
- Presentation rejeita chave desconhecida em vez de criar fallback visual.

### Supervisor, GUI e grafo

- N3 permanece sticky e fail-closed quando o SO nao prova encerramento.
- `FailedToStop` nao destroi um `QProcess` leader ainda vivo.
- O supervisor reabre somente depois de drain comprovado.
- A GUI exibe 30 logs/erros recentes, selecionaveis e copiaveis.
- Logs Qt persistem em tres arquivos rotativos de 1 MiB.
- Menus possuem contratos de efeito; pointer real por familia ainda e parcial.
- O grafo real cobre determinismo, bounds, overlap, centro, teclado e
  exportacao; URL Windows/UNC e fonte unica de geometria ainda sao pendencias.

### Performance medida

- Importacao de 250000 linhas passou com RSS adicional de 96387072 bytes no
  probe registrado.
- `qtd_derivadas`, 30 amostras: resumo 0.035/0.038 ms mediana/p95; fallback
  read-only com `GROUP BY` 10.168/10.574 ms.
- Cleanup de trigger por pai caiu de 10.143/10.701 ms para 0.255/0.309 ms.
- Prefetch, 30 execucoes: 73.572/74.577 ms e RSS maximo de 17661952 bytes,
  ainda incluindo startup/teardown QtTest.

## Validacao local autoritativa

- `cmake --preset dev`: passou.
- `cmake --build --preset dev`: passou.
- `cmake --build --preset dev --target all_qmllint`: passou.
- `ctest --preset dev --output-on-failure`: 443/443 em 71.39 segundos depois
  do bump para `0.9.10`.
- O caso de substituicao de fonte com mesmo tamanho/mtime passou 50/50.
- Suite presentation/filter e contratos CSV/CLI/staging passaram focados.
- Semgrep e detect-secrets: zero finding bloqueante.
- Gitleaks e TruffleHog: zero segredo encontrado.
- CodeRabbit final: 1 major e 1 minor, ambos corrigidos; gates repetidos.
- cppcheck manteve um falso positivo conhecido para referencia inicializada por
  `state_(state)` em `ColumnFilterViewModel`.

Os testes `ssa_gui_version` e `ssa_cli_version` passaram com `0.9.10`. A suite
443/443 acima prova o working tree da preparacao; commit, tag e publicacao sao
estados posteriores e devem ser verificados separadamente.

## Pendencias ativas priorizadas

1. Triar feedback de Cursor GLM e OpenCode com evidencia no HEAD.
2. Completar pointer real por familia de menu.
3. Fechar geometria unica e exportacao URL multiplataforma do grafo.
4. Isolar CPU/idle/latencia do prefetch e habilitar profiling valido.
5. Reparar `clang-tidy`, `.qmltypes` e timeout das regras Semgrep grandes.
6. Desenhar mitigacao por handle para TOCTOU residual de diretorio na
   consolidacao.

Detalhes, criterios e itens de prioridade menor ficam em
`RECOVERY_BACKLOG.md`. O plano executavel fica em
`docs/plans/2026-07-17-v0.9.10-follow-up.md`.

## Remotes e publicacao

| Remote | Provedor | Funcao | Estado antes da release |
| --- | --- | --- | --- |
| `origin` | GitLab | Repositorio e CI primarios | `d34f92d` confirmado |
| `bitbucket` | Bitbucket | Mirror obrigatorio | `d34f92d` confirmado |
| `gh` | GitHub | Mirror inativo | HTTP 403; conta suspensa |

Todo fetch ou pull operacional vem de `origin`:

```bash
git fetch origin master
git pull --ff-only origin master
```

Nunca usar Bitbucket ou GitHub como fonte de pull. Um pedido de commit inclui
push para `origin` e `bitbucket`, seguido de `git ls-remote` nos dois. Tags
novas tambem devem ser publicadas e verificadas nos dois remotes.

## Estado externo

- GitLab CI pode falhar instantaneamente por `ci_quota_exceeded`; isso nao e
  diagnostico de codigo.
- Bitbucket Pipelines permanece sujeito a cota mensal compartilhada.
- GitHub Actions e environment `release` continuam indisponiveis enquanto a
  conta responder HTTP 403.
- Windows, Linux e pacotes de distribuicao nao foram revalidados nesta
  preparacao documental.

Este snapshot distingue implementacao local, commit, validacao local e prova
externa. Execute novamente os comandos ao vivo antes de afirmar estado remoto.
