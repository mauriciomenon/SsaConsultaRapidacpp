# [MEDIUM] Mutable GitHub Action references are not pinned to immutable SHAs

**File:** [`.github/workflows/snyk-security.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/snyk-security.yml#L38-L77) (lines 38, 77)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-workflow-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The active workflow uses actions/checkout@v4 and github/codeql-action/upload-sarif@v3. These are mutable major-version tags, so a compromised upstream action or moved tag could change code executed in this job. The risk is higher because the job has security-events: write.

## Recommendation

Pin every action to a full commit SHA and use dependency tooling to review action updates explicitly.

## Revalidation

**Verdict:** true-positive

O workflow usa `actions/checkout@v4` e `github/codeql-action/upload-sarif@v3`, ambos por tags mutaveis. A acao Snyk setup esta pinada em SHA, mas isso nao protege os passos GitHub Action nao pinados. O job concede `security-events: write`, entao uma acao de upload SARIF comprometida poderia publicar resultados falsos ou omitir resultados reais no Code Scanning. Um checkout comprometido poderia alterar a arvore de trabalho antes dos comandos Snyk. Nao ha pinning por commit, allowlist local ou outra barreira no YAML. O achado e distinto do problema do Dockerfile porque aqui o vetor e supply chain da propria action, nao codigo controlado pelo PR. O arquivo nao foi corrigido desde sua criacao no historico local.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
