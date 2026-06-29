# [MEDIUM] CI uses mutable action refs, including third-party actions

**File:** [`.github/workflows/ci.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/ci.yml#L11-L101) (lines 11, 38, 48, 49, 90, 92, 95, 101)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-ci-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

Several CI steps use mutable major-version action refs: actions/checkout@v4, actions/upload-artifact@v4, jurplel/install-qt-action@v4, ilammy/msvc-dev-shell@v1, and actions/cache@v4. Because the workflow also builds and packages project code, a compromised or moved action tag could execute arbitrary code in the CI runner and access the job token and artifacts available to that job.

## Recommendation

Pin every action to a full commit SHA, especially third-party actions. Use Dependabot or another controlled update process to refresh pinned SHAs.

## Revalidation

**Verdict:** true-positive

O arquivo atual ainda usa actions/checkout@v4, actions/upload-artifact@v4, jurplel/install-qt-action@v4, ilammy/msvc-dev-shell@v1 e actions/cache@v4. Nenhum desses refs esta pinado a SHA completo, e nao ha mitigacao local como allowlist verificavel ou vendorizacao da action. Esses jobs compilam, testam e empacotam o projeto em macOS, Linux e Windows, inclusive gerando artefato macOS via upload-artifact. Um ataque concreto seria comprometer ou mover uma tag de action de terceiro, como jurplel/install-qt-action@v4 ou ilammy/msvc-dev-shell@v1, para executar codigo no runner antes do build e alterar toolchain, binarios ou artefatos. Mesmo actions oficiais por major tag continuam mutaveis do ponto de vista da politica de supply chain. O historico recente alterou CI e packaging, mas nao pinou refs nem adicionou controle equivalente. Isso confirma o achado como risco real de supply chain em CI.

## Recent committers (`git log`)

- Mauricio Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-06-26)
