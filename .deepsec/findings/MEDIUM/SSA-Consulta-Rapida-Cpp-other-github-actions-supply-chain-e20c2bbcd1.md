# [MEDIUM] Checkout usa ref mutavel

**File:** [`.github/workflows/pysa.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/pysa.yml#L34-L38) (lines 34, 38)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

O job concede `security-events: write` e usa `actions/checkout@v4`. Esse major tag e mutavel; se o ref upstream for comprometido ou movido, codigo nao revisado pode rodar antes da action Pysa fixada por SHA, com acesso ao token do job conforme permissoes declaradas.

## Recommendation

Fixar `actions/checkout` por SHA completo.

## Revalidation

**Verdict:** true-positive

O workflow Pysa roda em workflow_dispatch, push, pull_request e schedule, e o job concede actions: read, contents: read e security-events: write. O checkout usa actions/checkout@v4 com submodules: true, enquanto facebook/pysa-action esta fixada por SHA completo. O ref @v4 permanece mutavel, entao uma tag comprometida ou movida executaria codigo nao revisado antes da action Pysa. O impacto inclui leitura do repositorio e potencial interferencia no fluxo de code scanning conforme o token do job. O arquivo atual nao contem fix, e o historico consultado mostra apenas o commit que adicionou esse workflow.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
