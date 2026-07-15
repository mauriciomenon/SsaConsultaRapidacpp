# Status Da Rodada

Fonte operacional para humanos e agentes de codigo. Verifique este arquivo antes
de interpretar sincronizacao Git, status de CI ou falhas de publicacao.

Ultima verificacao: 2026-07-15

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

- Candidata local em preparacao: `v0.9.8`; `v0.9.7` permanece preservada.
- A tag deve apontar para o commit documental final desta rodada e ser publicada
  somente depois do pacote macOS e dos refs obrigatorios.
- Build canonico local e `ctest --preset dev --output-on-failure`: 377/377.
- `origin/master` e `bitbucket/master` foram verificados em
  `be16126a7c6bc6c1258f25cb649c3c8497d4860c` antes do commit documental.
- Pipeline GitLab da 0.9.7: pendente de verificacao apos o push final.
- Bitbucket Pipelines: manual e bloqueado pela cota mensal compartilhada ate a
  renovacao prevista em 2026-07-28. Nao tratar essa cota como falha de Git.
- GitHub Actions e o environment GitHub `release` sao externos e estao
  indisponiveis porque a conta GitHub esta suspensa.

Este e um snapshot datado. Execute novamente os comandos ao vivo antes de fazer
uma nova afirmacao de status externo.
