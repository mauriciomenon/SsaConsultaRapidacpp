# [MEDIUM] Checkout action is not pinned to an immutable SHA

**File:** [`.github/workflows/stackhawk.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/stackhawk.yml#L52) (lines 52)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-workflow-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow uses actions/checkout@v4, a mutable major-version tag. A moved or compromised upstream tag would change code executed in this workflow.

## Recommendation

Pin actions/checkout to a full commit SHA and update it through reviewed dependency changes.

## Revalidation

**Verdict:** true-positive

A linha 52 ainda usa actions/checkout@v4, que e uma tag major mutavel. O job roda em push, pull_request e schedule, com security-events: write definido no escopo da job. Se a tag v4 fosse movida ou comprometida, codigo alterado do checkout executaria dentro da job antes da etapa StackHawk. Esse codigo poderia abusar do token com permissoes do job ou persistir ate a etapa que recebe HAWK_API_KEY. Nao ha pin por SHA nem mecanismo local que limite a action checkout a uma versao imutavel. O finding nao depende da existencia de your-service.sh e representa uma classe diferente da execucao de codigo PR-controlado.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
