# [HIGH_BUG] CRDA PR runs scan the base branch instead of pull request changes

**File:** [`.github/workflows/crda.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/crda.yml#L10-L84) (lines 10, 11, 67, 84)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH_BUG  •  **Confidence:** high  •  **Slug:** `other-ci-security-gate-bypass`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow comments say PRs should be scanned, but the job uses pull_request_target with a default actions/checkout invocation. For pull_request_target, the default ref is the base branch, not the untrusted PR head. A PR that changes dependency manifests can therefore pass this CRDA check while the scanner analyzes old base content, creating a false security signal.

## Recommendation

For PR dependency analysis, run a no-secret pull_request workflow that checks out the PR SHA. If secrets are required, separate trusted reporting from untrusted scanning and do not combine PR-head checkout with repository secrets.

## Revalidation

**Verdict:** true-positive

O workflow declara pull_request_target e usa actions/checkout@v4 sem ref explicito nas linhas 67-84. Em pull_request_target, o SHA/ref padrao pertence ao contexto do branch base, nao ao head nao confiavel do PR. Assim, a area de trabalho que o CRDA analisa e o conteudo antigo do branch master, mesmo quando o PR altera manifests ou arquivos de dependencia. Um atacante pode abrir um PR com uma dependencia vulneravel ou manifest novo e receber o sinal de que a checagem CRDA passou porque ela analisou a base. Nao ha outro workflow pull_request sem secrets nesse arquivo que faca checkout do PR SHA. O historico do arquivo mostra apenas a adicao inicial, sem patch posterior para usar github.event.pull_request.head.sha ou separar scan sem segredo.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
