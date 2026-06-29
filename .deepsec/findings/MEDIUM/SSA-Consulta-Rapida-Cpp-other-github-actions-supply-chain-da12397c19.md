# [MEDIUM] Mutable scanner actions can poison code scanning results

**File:** [`.github/workflows/devskim.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/devskim.yml#L20-L32) (lines 20, 21, 22, 23, 26, 29, 32)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The job grants security-events: write and runs mutable action refs for checkout, DevSkim, and SARIF upload. If microsoft/DevSkim-Action@v1 or github/codeql-action/upload-sarif@v3 is compromised or retagged, the action can read the checked-out source and upload forged or incomplete SARIF results to the repository Security tab.

## Recommendation

Pin all actions to full commit SHAs and keep permissions limited to contents: read, actions: read, and security-events: write only for the upload job.

## Revalidation

**Verdict:** true-positive

I read the complete DevSkim workflow and confirmed it runs on push, pull_request, and a weekly schedule with actions: read, contents: read, and security-events: write. It checks out source with actions/checkout@v4, runs microsoft/DevSkim-Action@v1, then uploads devskim-results.sarif with github/codeql-action/upload-sarif@v3. All three action references are mutable version tags rather than full commit SHAs. The workflow gives the whole job security-events: write, so a compromised scanner action or upload action would be in the same trusted pipeline that writes code scanning results. A concrete attack is an upstream compromise or malicious retag of the DevSkim action that emits an incomplete or forged SARIF file, followed by the upload step publishing those results to the repository Security tab. A compromised upload action could also directly upload forged results while holding the same job permissions. The permissions are scoped better than a broad default token, but they still include the exact write capability needed for SARIF poisoning. The file has no patch after the cited introduction commit and no local diff, so the finding remains current.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
