# [MEDIUM] Pull request workflow executes PR-controlled Dockerfile with a write-scoped GitHub token

**File:** [`.github/workflows/snyk-security.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/snyk-security.yml#L24-L73) (lines 24, 32, 34, 38, 69, 70, 73)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `secrets-exposure`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs on pull_request, grants the job security-events: write, checks out the PR code with the default checkout credentials, then runs docker build on the repository context. A same-repo PR author could add or modify Dockerfile instructions to read checkout credentials from the workspace or use the job token context indirectly, then exfiltrate an ephemeral GitHub token with code-scanning write capability. Fork PRs have reduced secret exposure, but same-repo PRs from less-trusted collaborators remain a real CI trust-boundary risk.

## Recommendation

Do not build PR-controlled Dockerfiles in a secret/write-token job. Split PR scans into a read-only job, set checkout persist-credentials: false, remove security-events: write from steps that execute repository code, and run container monitoring only on trusted push or scheduled events.

## Revalidation

**Verdict:** true-positive

O workflow roda em `pull_request`, concede `security-events: write` ao job e usa `actions/checkout@v4` sem `persist-credentials: false`. Por padrao, checkout persiste o token do job nas credenciais Git locais, e nao ha `.dockerignore` rastreado para excluir `.git` do contexto Docker. Embora a branch base nao tenha Dockerfile, um PR do mesmo repositorio pode adicionar um Dockerfile que sera incluído no merge checkout usado pelo job. Esse Dockerfile pode copiar arquivos do contexto, inclusive configuracao Git com credenciais persistidas, e executar comandos durante `docker build`. Fork PRs tem reducoes de token e secrets, mas o cenario de PR de mesmo repositorio por colaborador menos confiavel permanece valido. O token tem permissao de escrita em Code Scanning, o que permite adulterar resultados SARIF ou estado relacionado a alertas. Nao encontrei mitigacao no workflow atual que separe build de codigo PR de permissoes de escrita.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
