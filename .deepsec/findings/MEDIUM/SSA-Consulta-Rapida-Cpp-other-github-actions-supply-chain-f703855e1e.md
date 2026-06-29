# [MEDIUM] SecurityCodeScan workflow uses mutable action refs

**File:** [`.github/workflows/securitycodescan.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/securitycodescan.yml#L24-L41) (lines 24, 26, 41)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

`actions/checkout@v4`, `microsoft/setup-msbuild@v1.0.2`, and `github/codeql-action/upload-sarif@v3` are tag refs rather than immutable commit SHAs. A moved or compromised tag can execute attacker-controlled workflow code in the CI context and tamper with scan results.

## Recommendation

Pin all actions to full commit SHAs and update them through reviewed dependency update PRs.

## Revalidation

**Verdict:** true-positive

O workflow usa actions/checkout@v4, microsoft/setup-msbuild@v1.0.2 e github/codeql-action/upload-sarif@v3 por refs de tag. Outras actions no mesmo arquivo estao fixadas por SHA, entao o arquivo demonstra que o pinning por SHA e possivel mas incompleto. Tags de actions podem ser movidas ou comprometidas upstream, e nesse caso o codigo da action roda no runner antes ou durante a geracao e envio de resultados. Mesmo com a incerteza sobre permissoes implicitas do token, a falha de supply chain existe no arquivo atual e afeta o fluxo de code scanning. O git diff esta limpo para esse workflow e nao ha commit posterior no historico corrigindo os refs.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
