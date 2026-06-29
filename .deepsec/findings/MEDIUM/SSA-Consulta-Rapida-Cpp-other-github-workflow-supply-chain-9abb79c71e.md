# [MEDIUM] Several active actions use mutable major-version tags

**File:** [`.github/workflows/xanitizer.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/xanitizer.yml#L60-L98) (lines 60, 65, 90, 98)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-workflow-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow uses actions/checkout@v4, actions/setup-java@v4, actions/upload-artifact@v4, and github/codeql-action/upload-sarif@v3. These mutable refs can change without review if an upstream tag is moved or compromised. The third-party Xanitizer action itself is pinned to a SHA.

## Recommendation

Pin all GitHub Actions to full commit SHAs and review updates explicitly.

## Revalidation

**Verdict:** true-positive

O workflow ainda usa actions/checkout@v4, actions/setup-java@v4, actions/upload-artifact@v4 e github/codeql-action/upload-sarif@v3. Essas refs sao tags mutaveis, enquanto a action RIGS-IT/xanitizer-action esta corretamente fixada por SHA. O job tem security-events: write e actions: read, e tambem passa XANITIZER_LICENSE para uma etapa posterior. Um comprometimento ou movimento de tag em qualquer dessas actions pode mudar o codigo executado sem revisao no repositorio. As actions anteriores ao segredo podem persistir processo para observar a etapa secreta; a action de upload SARIF pode tamperar resultados de code scanning. Nao ha pin por SHA nas actions listadas.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
