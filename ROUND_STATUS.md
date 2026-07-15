# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo antes
de interpretar sincronizacao Git, status de CI ou falhas de publicacao.

Ultima verificacao: 2026-07-15

## Prioridades de importacao v0.9.9 em andamento

Esta matriz registra somente evidencias do checkout local. Os commits abaixo
ainda nao foram publicados nos remotes nesta rodada.

| Prioridade | Status | Evidencia |
| --- | --- | --- |
| 1. Preservar banco em full/incremental | ENTREGUE | rollback e merge nao destrutivo mantidos; suite de integracao verde |
| 2. Validar schema compartilhado | ENTREGUE | `ColumnCatalog` usado por GUI, CLI, mapper e validador |
| 3. Recriar DB sem apagar o atual | ENTREGUE | teste renomeia backup em temporario e valida DB antigo e novo |
| 4. Datas e metadata | ENTREGUE | data planilha, nome, criacao quando disponivel e mtime; parser date-only adicionado |
| 5. Perfil de fonte | ENTREGUE PARCIAL | `executadas` vence empate; derivadas/desvios ainda enriquecem por campos |
| 6. TOCTOU na copia | ENTREGUE PARCIAL | tamanho e mtime comparados antes do rename; identidade inode/handle permanece futura |
| 7. Lock entre raizes para o mesmo DB | ENTREGUE PARCIAL | lock de corpus mais lock hash global por caminho absoluto; aliases fisicos e colisao de hash ainda nao sao provados |
| 8. Cancelamento e staging | ENTREGUE PARCIAL | stop remove temporarios e sweep de `.ssa-staged-`; janela SIGKILL pos-publicacao sem prova deterministica |
| 9. SAM ate SQLite | ENTREGUE LOCAL PARCIAL | adapter, manifesto, truncamento, rollback e importacao em SQLite passam; REST real permanece dependencia externa |
| 10. Derivadas explicitas | ENTREGUE LOCAL PARCIAL | CSV/TXT/TSV/XLSX/XLSM e cancelamento passam; limpeza de orfas segue acao separada |
| 11. Auxiliar Arrow | ENTREGUE | preset `dev-arrow` e inspector somente leitura em v0.9.8 |
| 12. GUI/CLI e dicionario | ENTREGUE | schema e colunas obrigatorias vem do dominio; validador exige schema integral |
| 13. Release e remotes | PENDENTE | HEAD local avancou; tag v0.9.8 permanece preservada; push depende de SSH |

### Commits funcionais locais

- `8354a62` dicionario e validacao do schema SQLite.
- `8092e8c` coluna `data_cadastro` obrigatoria no header.
- `bac398c` lock por identidade do banco.
- `ea43800` rejeicao de fonte alterada durante copia.
- `b8957af` prioridade de fontes executadas em empate.
- `a918f93` mapper consumindo colunas obrigatorias compartilhadas.
- `af32c50` parser de data sem hora no nome da planilha.

### Validacao local desta rodada

- `ssa_unit_tests`: 569 asserts em 108 casos.
- `ssa_integration_tests`: 5636 asserts em 247 casos nesta execucao; alguns
  testes de polling variam a contagem de asserts, por isso o gate canonico e
  `ctest`.
- `ctest --preset dev --output-on-failure`: 391/391.
- Recriacao e validacao do DB: 38 asserts no caso dedicado; o banco real nao
  foi tocado.
- O staged `tests/smoke/AdvancedPopupQmlTest.cpp` e os untracked do usuario
  continuam fora dos commits.

## Remotes Git

| Remote | Provedor | Funcao | Estado atual |
| --- | --- | --- | --- |
| `origin` | GitLab | Repositorio e CI primarios | Disponivel |
| `bitbucket` | Bitbucket | Mirror obrigatorio de push; nao usar para pull | Disponivel |
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

## Release e CI atuais

- Release local concluida: tag anotada `v0.9.8`; `v0.9.7` permanece preservada.
- A tag deve apontar para o commit documental final desta rodada e ser publicada
  somente depois do pacote macOS e dos refs obrigatorios.
- Build canonico local e `ctest --preset dev --output-on-failure`: 379/379.
- Pacote macOS arm64 v0.9.8 gerado; ZIP e DMG aguardam publicacao junto com a
  tag porque os dois remotes SSH estao em timeout nesta rodada.
- `origin/master` e `bitbucket/master` foram verificados em
  `be16126a7c6bc6c1258f25cb649c3c8497d4860c` antes do commit documental.
- Pipeline GitLab da 0.9.7: pendente de verificacao apos o push final.
- Bitbucket Pipelines: manual e bloqueado pela cota mensal compartilhada ate a
  renovacao prevista em 2026-07-28. Nao tratar essa cota como falha de Git.
- GitHub Actions e o environment GitHub `release` sao externos e estao
  indisponiveis porque a conta GitHub esta suspensa.

Este e um snapshot datado. Execute novamente os comandos ao vivo antes de fazer
uma nova afirmacao de status externo.
