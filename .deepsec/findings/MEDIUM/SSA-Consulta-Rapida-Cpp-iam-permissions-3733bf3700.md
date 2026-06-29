# [MEDIUM] CI workflow lacks explicit GITHUB_TOKEN permission hardening

**File:** [`.github/workflows/ci.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/ci.yml#L3-L117) (lines 3, 5, 11, 15, 17, 34, 37, 48, 58, 60, 90, 112, 117)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `iam-permissions`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs on pull_request and executes repository-controlled build, test, and packaging commands, but it does not declare a top-level or job-level permissions block. If the repository default token permission is write or if same-repository PRs are allowed, PR-controlled scripts can run with a broader GITHUB_TOKEN than needed. actions/checkout also persists credentials by default, making the token reachable through the local git config in later run steps.

## Recommendation

Add explicit least-privilege permissions, normally permissions: contents: read for this CI workflow. Set persist-credentials: false on checkout unless a later step truly needs git credentials.

## Revalidation

**Verdict:** uncertain

O arquivo atual nao declara permissions no topo nem nos jobs macos, linux ou windows. Os jobs executam actions/checkout@v4 e depois comandos controlados pelo repositorio, como cmake, ctest, scripts/package-macos.sh e smoke tests, de modo que codigo de PR pode influenciar comandos executados pelo runner. O checkout tambem nao define persist-credentials: false, entao as credenciais do token podem ficar disponiveis para etapas run posteriores por meio da configuracao local do git. Um ataque concreto existiria se o GITHUB_TOKEN efetivo tiver permissoes de escrita, pois um PR de mesmo repositorio ou uma alteracao maliciosa em scripts/build poderia ler e abusar desse token. A parte ambigua e que as permissoes efetivas dependem da configuracao externa do repositorio ou organizacao, que nao esta no codigo e nao pode ser verificada estaticamente aqui. Para PRs de forks, o GitHub normalmente restringe o token e nao expoe segredos, o que pode reduzir bastante a explorabilidade. Portanto o YAML esta sem hardening e deve ser corrigido, mas nao da para confirmar explorabilidade atual sem a politica real de GITHUB_TOKEN do repositorio.

## Recent committers (`git log`)

- Mauricio Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-06-26)
