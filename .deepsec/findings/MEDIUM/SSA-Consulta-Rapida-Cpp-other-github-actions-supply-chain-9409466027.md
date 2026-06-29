# [MEDIUM] Snyk IaC workflow uses mutable actions around security scan results

**File:** [`.github/workflows/snyk-infrastructure.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/snyk-infrastructure.yml#L29-L52) (lines 29, 31, 35, 52)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The Snyk action is pinned to a commit SHA, and `SNYK_TOKEN` is supplied only to that step, but checkout and SARIF upload use mutable major-version tags. A compromised checkout action can change what is scanned, and a compromised SARIF upload action can tamper with code scanning results using the job's `security-events: write` permission.

## Recommendation

Pin `actions/checkout` and `github/codeql-action/upload-sarif` to full commit SHAs. Keep the Snyk token scoped to the pinned Snyk step.

## Revalidation

**Verdict:** true-positive

O workflow completo confirma que a acao Snyk IaC esta pinada em SHA, enquanto `actions/checkout@v4` e `github/codeql-action/upload-sarif@v3` usam tags mutaveis. O job tem `security-events: write`, e o upload SARIF roda nesse mesmo job. Se a tag mutavel do upload SARIF fosse comprometida ou movida, o codigo da acao poderia manipular ou substituir os resultados enviados ao GitHub Code Scanning. Se a tag de checkout fosse comprometida, ela poderia modificar o conteudo analisado antes da etapa Snyk. Nao ha pinning por SHA nesses dois passos nem outra mitigacao no arquivo. O historico mostra que o workflow nao recebeu correcao apos a criacao. Portanto o risco de integridade de scan por supply chain e real dentro do modelo de ameaca de GitHub Actions.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
