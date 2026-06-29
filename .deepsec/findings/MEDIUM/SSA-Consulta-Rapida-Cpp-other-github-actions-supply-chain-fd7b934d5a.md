# [MEDIUM] Refs mutaveis em GitHub Actions

**File:** [`.github/workflows/flawfinder.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/flawfinder.yml#L24-L36) (lines 24, 27, 36)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

O workflow executa em push, pull_request e schedule com `security-events: write`, mas usa `actions/checkout@v4` e `github/codeql-action/upload-sarif@v3`. Esses major tags sao mutaveis; se uma tag upstream for movida ou comprometida, codigo nao revisado pode rodar com leitura do repositorio e permissao de escrita em code scanning. O scanner flawfinder em si esta fixado por SHA, mas essas duas actions oficiais nao estao.

## Recommendation

Fixar todas as actions por SHA completo, incluindo `actions/checkout` e `github/codeql-action/upload-sarif`, e usar automacao controlada para atualizacoes.

## Revalidation

**Verdict:** true-positive

I read the full flawfinder workflow and confirmed it runs on push, pull_request, and schedule with actions: read, contents: read, and security-events: write. The scanner action david-a-wheeler/flawfinder is pinned to a full commit SHA, which is a useful mitigation for that specific action. However, the workflow still uses actions/checkout@v4 and github/codeql-action/upload-sarif@v3 as mutable major-version tags. A compromised or retagged checkout action would execute before scanning with repository read access, and a compromised upload-sarif action would execute with the job permission needed to write code scanning results. The concrete impact is forged, missing, or manipulated SARIF being uploaded to the Security tab under a trusted workflow. The file does not split upload into a separate trusted context or pin all actions by SHA. Git history for this target only contains the add-workflow commit, and there is no local diff showing a fix. The finding is therefore real despite the flawfinder scanner itself being pinned.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
