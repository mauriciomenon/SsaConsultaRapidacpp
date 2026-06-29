# [MEDIUM] Mutable checkout action runs in a token-bearing security job

**File:** [`.github/workflows/bandit.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/bandit.yml#L25-L39) (lines 25, 26, 27, 28, 32, 39)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-ci-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The job runs actions/checkout@v4 by mutable major tag, then passes GITHUB_TOKEN to the Bandit action. The Bandit action itself is pinned to a commit SHA, but checkout is not. A moved or compromised checkout ref would execute code in the job context and receive the implicit GitHub token used by checkout, with the job permissions shown here.

## Recommendation

Pin actions/checkout to a full commit SHA. Prefer using github.token explicitly only where required and set persist-credentials: false when later steps do not need git credentials.

## Revalidation

**Verdict:** true-positive

O arquivo atual ainda usa actions/checkout@v4 na linha 32, sem pin para SHA completo e sem persist-credentials: false. O job declara permissions com contents: read, security-events: write e actions: read nas linhas 25-28. A action Bandit esta pinada por SHA, mas o token do GitHub e passado explicitamente para ela na linha 39. O workflow roda em push, pull_request e schedule, entao uma action upstream comprometida teria execucao em um job com token disponivel. Um ataque concreto seria mover ou comprometer o ref actions/checkout@v4 para executar codigo malicioso antes da varredura, ler o token usado pelo checkout e abusar das permissoes do job, incluindo manipulacao de resultados de security-events. A permissao contents: read limita escrita no repositorio, mas nao remove o risco de token e de adulteracao de sinal de seguranca. Nao encontrei mitigacao versionada ou mudanca posterior no historico que corrija isso.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
