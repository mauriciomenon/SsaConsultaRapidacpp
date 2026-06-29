# [MEDIUM] Refs mutaveis em GitHub Actions

**File:** [`.github/workflows/njsscan.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/njsscan.yml#L27-L40) (lines 27, 33, 40)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

O workflow executa em push, pull_request e schedule com `security-events: write`, mas usa `actions/checkout@v4` e `github/codeql-action/upload-sarif@v3`. Esses major tags sao mutaveis; um comprometimento ou retarget upstream pode executar codigo nao revisado com leitura do repositorio e escrita em code scanning. A action `ajinabraham/njsscan-action` esta fixada por SHA, mas as actions oficiais nao.

## Recommendation

Substituir os major tags por SHAs completos para todas as actions e revisar periodicamente via Dependabot/Renovate.

## Revalidation

**Verdict:** true-positive

I read the complete njsscan workflow and confirmed it runs on push, pull_request, and schedule. The top-level permissions set contents: read, while the job grants contents: read, security-events: write, and actions: read. The njsscan action itself is pinned to a full commit SHA, but actions/checkout@v4 and github/codeql-action/upload-sarif@v3 remain mutable major-version tags. A malicious retag or upstream compromise of checkout could run code in the job before scanning, and a compromised upload-sarif action could publish forged or incomplete results using security-events: write. The scanner step also uses args ending in || true, so a scan failure is intentionally tolerated before upload, which makes result integrity especially dependent on the trusted upload path and SARIF file contents. There is no local mitigation such as full-SHA pinning for all actions or a separated permission boundary for upload. Git history shows only the cited introduction commit for this file and no later patch. The issue is not a duplicate because duplicate classification is limited to findings within the same file, and this is a separate workflow location.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
