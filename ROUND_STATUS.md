# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo antes
de interpretar sincronizacao Git, status de CI ou falhas de publicacao.

Ultima verificacao: 2026-07-16

## Prioridades de importacao pos-v0.9.9

Esta matriz registra evidencias do checkout local e dos refs remotos verificados.
Os commits abaixo pertencem ao baseline historico. Nenhum commit, push, tag ou
PR foi criado nesta rodada; a atividade atual esta descrita abaixo.

### Atualizacao desta rodada

- HEAD local: `07c1e44` em `master`; `origin/master` e `bitbucket/master`
  apontam para o mesmo commit.
- Working tree: 18 arquivos modificados e 8 untracked; nenhum arquivo staged.
  Arquivos locais e artefatos permanecem fora do escopo de publicacao.
- `ctest --preset dev -N`: 419 testes registrados nesta configuracao.
- Gate final `ctest --preset dev --output-on-failure`: 419/419 passaram em
  58.04 segundos; build canonico e `all_qmllint` tambem passaram.
- Validacao focada: rescan/importacao 125 casos, N8/grafo/SAM 46/46, suites
  presentation e popup 2/2, menus 1/1 e prefetch 7/7.
- Probes existentes: importacao de 250000 linhas passou com RSS adicional de
  96387072 bytes; popup RSS passou nos modos eager e virtual.
- Scans imediatos dos diffs: clang-format, semgrep, detect-secrets e os gates
  aplicaveis passaram; cppcheck nao encontrou arquivos elegiveis.
- Nenhum commit, push, tag ou PR foi realizado nesta rodada.

| Prioridade | Status | Evidencia |
| --- | --- | --- |
| 1. Preservar banco em full/incremental | ENTREGUE | rollback e merge nao destrutivo mantidos; suite de integracao verde |
| 2. Validar schema compartilhado | ENTREGUE | `ColumnCatalog` usado por GUI, CLI, mapper e validador |
| 3. Recriar DB sem apagar o atual | ENTREGUE | teste renomeia backup em temporario e valida DB antigo e novo |
| 4. Datas e metadata | ENTREGUE | data planilha, nome, criacao quando disponivel e mtime; parser date-only adicionado |
| 5. Perfil de fonte | ENTREGUE PARCIAL | `executadas` vence empate; derivadas/desvios enriquecem por campos ricos; corpus real ainda nao foi reprocessado |
| 6. TOCTOU na copia | ENTREGUE | tamanho, mtime e identidade do arquivo comparados antes do rename; teste de substituicao com mesmo tamanho/mtime passa |
| 7. Lock entre raizes para o mesmo DB | ENTREGUE | lock de corpus mais lock global por caminho canonico com SHA-256; teste cobre duas raizes e alias por symlink |
| 8. Cancelamento e staging | ENTREGUE PARCIAL | stop remove temporarios e sweep de `.ssa-staged-`; janela SIGKILL pos-publicacao sem prova deterministica |
| 9. SAM ate SQLite | ENTREGUE LOCAL PARCIAL | adapter, manifesto, truncamento, rollback e importacao em SQLite passam; REST real permanece dependencia externa |
| 10. Derivadas explicitas | ENTREGUE LOCAL PARCIAL | CSV/TXT/TSV/XLSX/XLSM e cancelamento passam; limpeza de orfas segue acao separada |
| 11. Auxiliar Arrow | ENTREGUE | preset `dev-arrow` e inspector somente leitura em v0.9.8 |
| 12. GUI/CLI e dicionario | ENTREGUE | schema e colunas obrigatorias vem do dominio; validador exige schema integral |
| 13. Release e remotes | BASELINE HISTORICO | `master` e `v0.9.9` foram conferidos em rodada anterior; nenhum release foi publicado agora |

### Commits funcionais historicos do baseline

- `8354a62` dicionario e validacao do schema SQLite.
- `8092e8c` coluna `data_cadastro` obrigatoria no header.
- `bac398c` lock por identidade do banco.
- `ea43800` rejeicao de fonte alterada durante copia.
- `b8957af` prioridade de fontes executadas em empate.
- `a918f93` mapper consumindo colunas obrigatorias compartilhadas.
- `af32c50` parser de data sem hora no nome da planilha.
- `f55f0d9` lock por caminho canonico e digest SHA-256, incluindo alias por symlink.
- `3278646` preservacao de campos ricos em estados terminais.
- `bab8616` estados excepcionais com descricao.
- `51c05be` identidade de arquivo no staging.
- `ac78a51` precedencia de emissao e fallback de issue.
- `e26bfb1` icone rastreado e integrado a builds/pacotes.

### Validacao local desta rodada

- `git ls-remote --heads origin refs/heads/master` e Bitbucket: ambos apontam
  para `07c1e44`.
- O build dos targets `ssa_integration_tests`, `ssa_qt_sam_refresh_tests`,
  `ssa_qml_advanced_popup_tests`, `ssa_unit_tests`, probes de RSS passou.
- `ctest --preset dev -R 'rescan|consolidation|spreadsheet.*workflow|workflow.*spreadsheet'`:
  54/54.
- O caso N1 e a suite `ssa_qt_sam_refresh_tests` passaram sem o teste candidato
  que mantinha processo vivo; falhas de parada permanecem em casos separados.
- O caso de trim Qt-free passou em `ssa_unit_tests`.
- As tres copias identicas de trim na importacao usam agora o contrato Qt-free
  `domain/WhitespaceTrim.h`; variantes de parser e `string_view` nao mudaram.
- `ctest --preset dev -R '^ssa_qml_advanced_popup_tests$'` com
  `QT_QPA_PLATFORM=offscreen`: passou.
- O grafo real passou determinismo, bounds, ausencia de overlap, centro nas duas
  orientacoes e exportacao PNG decodificavel; teclado e SVG permanecem verdes.
- `ctest --preset dev -R '^ssa_qml_popup_rss$|^ssa_import_rss_250k$'`: 2/2.
- Microbenchmark local de `qtd_derivadas`, 250000 linhas e 30 amostras:
  resumo 0.035/0.038 ms mediana/p95, coluna omitida 0.012/0.013 ms, fallback
  read-only com `GROUP BY` 10.168/10.574 ms e conexao cold 0.444/0.571 ms.
  Inicializacao do resumo ficou em 10.079/11.164 ms; trigger de insert de 100
  linhas em 0.269/0.413 ms e delete de 100 em 10.143/10.701 ms.
- Prefetch: 30 execucoes do runner ficaram em 73.572/74.577 ms e RSS maximo de
  17661952 bytes, incluindo startup/teardown QtTest. Os contratos cold, paginas
  2/3, cache hit, fingerprint, generation, cancelamento e latest-wins passaram.
- `qmlprofiler` foi tentado, mas o target QtTest nao possui QML debugging; a
  ferramenta recusou a gravacao e marcou o trace como danificado.
- CodeRabbit autenticou, mas a revisao final ficou bloqueada pelo rate limit da
  cota CLI gratuita. Isso e dependencia externa, nao validacao concluida.
- A credencial local em `zshrc20260712` nao foi lida, editada ou versionada;
  o proprietario deve providenciar a rotacao fora deste checkout.
- A verificacao de `markdownlint-cli2` nao foi executada porque a ferramenta nao
  esta instalada neste ambiente.
- Recriacao e validacao do DB: 38 asserts no caso dedicado; o banco real nao
  foi tocado.
- Nao existe arquivo staged. Os diffs e untracked continuam no working tree e
  nenhum deles foi commitado ou publicado.

## Remotes Git e estado desta rodada

| Remote | Provedor | Funcao | Estado atual |
| --- | --- | --- | --- |
| `origin` | GitLab | Repositorio e CI primarios | refs alinhados; nenhuma publicacao nesta rodada |
| `bitbucket` | Bitbucket | Mirror obrigatorio de push; nao usar para pull | refs alinhados; nenhuma publicacao nesta rodada |
| `gh` | GitHub | Mirror inativo | HTTP 403 enquanto a conta esta suspensa |

O branch deste repositorio e `master`, nao `dev` ou `main`.

## Fonte de atualizacao local

Todo fetch ou pull operacional vem do GitLab por `origin`. O comando padrao e:

```bash
git fetch origin master
git pull --ff-only origin master
```

Nao executar `git pull bitbucket` nem `git pull gh`. O Bitbucket recebe os mesmos
commits e tags por push, mas nao e a fonte de integracao do checkout local.

`origin` nunca significa GitHub neste repositorio. Por exemplo:

```bash
git fetch origin master
git rev-list --count --left-right origin/master...HEAD
```

O resultado acima compara o `HEAD` local com o ref de acompanhamento do GitLab
atualizado por `git fetch`. O resultado `0 0` prova sincronizacao somente com o
GitLab.

Da mesma forma, este comando nao contata o GitHub:

```bash
git rev-list --count --left-right gh/master...HEAD
```

Ele compara o ref `gh/master` armazenado localmente e pode relatar divergencia
mesmo quando o GitHub esta inacessivel. Use `git ls-remote` para verificar um
provedor ao vivo:

```bash
git ls-remote --heads origin refs/heads/master
git ls-remote --heads bitbucket refs/heads/master
git ls-remote --heads gh refs/heads/master
```

Na ultima verificacao, os dois primeiros comandos passaram e `gh` retornou HTTP
403 com `Your account is suspended`.

## Politica de commit e publicacao

Quando o usuario pede `commit`, a operacao esperada e:

1. Criar o commit dentro do escopo no branch atual.
2. Publicar o branch em `origin` e `bitbucket`.
3. Publicar novas tags de release em `origin` e `bitbucket`.
4. Verificar os dois refs remotos com `git ls-remote`.
5. Nao publicar em `gh` ate o usuario reabilitar explicitamente o GitHub.

Nunca relatar um commit como totalmente publicado quando somente um dos dois
remotes ativos o aceitou.

## Historico de Release e CI

- A tag anotada `v0.9.9` e os pacotes macOS arm64 pertencem ao historico de
  release anterior; nenhuma release foi gerada nesta rodada.
- A configuracao local atual registra 419 testes; o resultado do gate completo
  foi 419/419 em 58.04 segundos. Esse registro local nao implica commit ou
  publicacao.
- Pipeline GitLab `2679255778`: jobs `secret-scan` e `linux-verify` falharam
  por `ci_quota_exceeded`, sem diagnostico de codigo.
- Bitbucket Pipelines: manual e bloqueado pela cota mensal compartilhada ate a
  renovacao prevista em 2026-07-28. Nao tratar essa cota como falha de Git.
- GitHub Actions e o environment GitHub `release` sao externos e estao
  indisponiveis porque a conta GitHub esta suspensa.

Este e um snapshot datado. Execute novamente os comandos ao vivo antes de fazer
uma nova afirmacao de status externo.
