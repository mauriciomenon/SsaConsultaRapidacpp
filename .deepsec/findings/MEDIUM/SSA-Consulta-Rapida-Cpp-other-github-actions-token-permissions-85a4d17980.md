# [MEDIUM] Mutable actions run with inherited GITHUB_TOKEN permissions

**File:** [`.github/workflows/defender-for-devops.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/defender-for-devops.yml#L21-L45) (lines 21, 24, 29, 35, 36, 42, 45)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `other-github-actions-token-permissions`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

This workflow has no explicit permissions block, so GITHUB_TOKEN inherits the repository default. The job checks out PR code and runs mutable action refs, including actions/checkout@v4, actions/setup-dotnet@v4, microsoft/security-devops-action@v1.6.0, and github/codeql-action/upload-sarif@v3. If any referenced action tag is compromised or moved, the attacker gets whatever default token permissions the repository grants, which can be broader than the SARIF upload task requires.

## Recommendation

Add least-privilege permissions such as contents: read, security-events: write, and actions: read if needed, then pin all actions to full commit SHAs.

## Revalidation

**Verdict:** uncertain

O arquivo atual nao declara permissions no topo nem no job MSDO, entao o GITHUB_TOKEN herda a politica padrao do repositorio ou organizacao. Ele usa actions/checkout@v4, actions/setup-dotnet@v4, microsoft/security-devops-action@v1.6.0 e github/codeql-action/upload-sarif@v3, todos por refs mutaveis. Tambem roda em push, pull_request e schedule, e faz checkout do codigo antes das actions de seguranca. Um ataque concreto existiria se uma dessas tags fosse comprometida e o token padrao tivesse permissoes alem do necessario, por exemplo security-events: write para adulterar upload SARIF ou outras permissoes de escrita herdadas. Porem a permissao efetiva do token nao esta definida no YAML e depende de configuracao externa do repositorio, que nao consegui verificar por analise estatica. Se a politica externa for read-only restritiva, o impacto de token pode ser menor, embora o risco de action mutavel ainda permaneça. Assim, o arquivo esta sem hardening e nao esta corrigido, mas a explorabilidade exata do componente de permissoes herdadas e incerta sem os settings do Actions.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
