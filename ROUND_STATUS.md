# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo antes
de interpretar sincronizacao Git, status de CI ou falhas de publicacao.

Ultima verificacao: 2026-07-15

## Prioridades de importacao pos-v0.9.9

Esta matriz registra evidencias do checkout local e dos refs remotos verificados.
Os commits abaixo foram publicados nos dois remotes nesta rodada.

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
| 13. Release e remotes | ENTREGUE | `master` e `v0.9.9` conferidos em origin e bitbucket via HTTPS |

### Commits funcionais locais

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

- `ctest --preset dev --output-on-failure`: 396/396.
- Testes focados de politica, mapper, staging e identidade passaram antes do
  gate completo; o build canonico foi executado pelo script versionado.
- `git ls-remote --heads origin master` e Bitbucket: refs conferidos via HTTPS
  apos a publicacao do commit de release `40737b4`.
- `git ls-remote --tags origin v0.9.9` e Bitbucket: tag anotada conferida e
  objeto apontando para `40737b4`.
- `markdownlint-cli 0.49.0`: changelog, status, roadmap e release notes sem
  ocorrencias.
- Recriacao e validacao do DB: 38 asserts no caso dedicado; o banco real nao
  foi tocado.
- O staged `tests/smoke/AdvancedPopupQmlTest.cpp` e os untracked do usuario
  continuam fora dos commits.

## Remotes Git

| Remote | Provedor | Funcao | Estado atual |
| --- | --- | --- | --- |
| `origin` | GitLab | Repositorio e CI primarios | HTTPS autenticado por glab/keychain; release publicada |
| `bitbucket` | Bitbucket | Mirror obrigatorio de push; nao usar para pull | HTTPS autenticado por GCM/keychain; release publicada |
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

- Release local e remota: tag anotada `v0.9.9`; `v0.9.8` e `v0.9.7` permanecem
  preservadas.
- A tag local aponta para o commit documental final desta rodada.
- Build canonico local e `ctest --preset dev --output-on-failure`: 396/396.
- Pacote macOS arm64 v0.9.9 foi gerado apos os gates locais; ZIP e DMG foram
  verificados com o icon no bundle.
- A tag `v0.9.9` esta presente nos dois remotes e aponta para `40737b4`.
  O branch `master` recebeu este status documental depois da release.
- Pipeline GitLab `2679255778`: jobs `secret-scan` e `linux-verify` falharam
  por `ci_quota_exceeded`, sem diagnostico de codigo.
- Bitbucket Pipelines: manual e bloqueado pela cota mensal compartilhada ate a
  renovacao prevista em 2026-07-28. Nao tratar essa cota como falha de Git.
- GitHub Actions e o environment GitHub `release` sao externos e estao
  indisponiveis porque a conta GitHub esta suspensa.

Este e um snapshot datado. Execute novamente os comandos ao vivo antes de fazer
uma nova afirmacao de status externo.
