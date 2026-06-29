# [MEDIUM] Workflow relies on implicit token permissions while running pull request code

**File:** [`.github/workflows/securitycodescan.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/securitycodescan.yml#L11-L41) (lines 11, 14, 20, 23, 24, 31, 32, 34, 35, 41)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `iam-permissions`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

This workflow has no top-level or job-level `permissions` block. It runs on `pull_request`, checks out the pull request code, restores/builds it, and uploads SARIF. If the repository default token permissions are permissive, the job may expose more GitHub token capability than needed to code running during restore/build and to mutable action refs.

## Recommendation

Add explicit least-privilege permissions, for example `contents: read`, `security-events: write`, and `actions: read` only if required. Set `persist-credentials: false` on checkout unless a later step needs Git credentials.

## Revalidation

**Verdict:** uncertain

O arquivo realmente nao possui bloco permissions top-level nem job-level, entao o GITHUB_TOKEN depende das configuracoes padrao do repositorio ou organizacao. O workflow roda em push, pull_request e schedule, faz checkout do codigo e depois executa dotnet restore e dotnet build, que poderiam processar projetos adicionados por uma PR. Porem a permissao efetiva do token nao e visivel no codigo, e GitHub tambem aplica restricoes diferentes para pull requests de forks. Alem disso, o repositorio atual nao contem projetos .NET rastreados, entao o caminho de execucao de build de codigo .NET nao existe na base atual, embora uma PR possa introduzi-lo. A ausencia de permissions e uma configuracao ruim, mas a explorabilidade concreta depende de estado externo que nao da para confirmar por analise estatica do repositorio.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
