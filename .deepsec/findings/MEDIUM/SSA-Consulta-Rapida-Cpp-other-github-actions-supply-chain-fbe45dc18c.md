# [MEDIUM] Semgrep workflow leaves scan integrity dependent on mutable action tags

**File:** [`.github/workflows/semgrep.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/semgrep.yml#L27-L46) (lines 27, 29, 35, 46)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The Semgrep action itself is pinned to a commit SHA and the `secrets.*` references are not exposed in source, but checkout and SARIF upload use mutable major-version tags. A compromised `actions/checkout@v4` could alter the workspace before scanning, and a compromised `github/codeql-action/upload-sarif@v3` could alter uploaded results while holding `security-events: write`.

## Recommendation

Pin `actions/checkout` and `github/codeql-action/upload-sarif` to full commit SHAs. Keep Semgrep tokens in GitHub Secrets and avoid exposing them to any unpinned third-party action.

## Revalidation

**Verdict:** true-positive

Li o workflow completo e confirmei que `returntocorp/semgrep-action` esta fixado em SHA, mas `actions/checkout@v4` e `github/codeql-action/upload-sarif@v3` continuam em tags mutaveis. O job concede `security-events: write` ao mesmo contexto em que o upload SARIF executa. Um comprometimento ou retag upstream de `upload-sarif@v3` permitiria executar codigo no job e adulterar resultados enviados ao Code Scanning. Um comprometimento de `checkout@v4` tambem poderia alterar o workspace antes da analise, fazendo o Semgrep analisar conteudo diferente do esperado. Nao ha mitigacao local como pin por SHA ou isolamento adicional para esses passos. O historico local mostra apenas a criacao do arquivo e `git diff` nao mostra patch posterior, entao o achado permanece atual. A explorabilidade depende de supply chain upstream, nao de um PR comum, mas esse e exatamente o modelo de risco descrito pelo achado.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
