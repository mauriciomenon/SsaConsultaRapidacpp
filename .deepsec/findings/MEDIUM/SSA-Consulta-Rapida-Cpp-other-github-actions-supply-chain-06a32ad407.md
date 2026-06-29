# [MEDIUM] Mutable dependency review action runs with pull request write permission

**File:** [`.github/workflows/dependency-review.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/dependency-review.yml#L21-L36) (lines 21, 22, 24, 31, 33, 36)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow grants pull-requests: write for PR comments and then runs mutable action refs, including actions/checkout@v4 and actions/dependency-review-action@v4. A compromised or retagged action can use that token capability to write misleading PR comments or manipulate dependency review output, and can read repository contents from the checkout.

## Recommendation

Pin actions to full commit SHAs. If PR comments are not required, remove pull-requests: write and disable comment-summary-in-pr; otherwise keep the permission scoped only to this job.

## Revalidation

**Verdict:** true-positive

I read the full workflow and confirmed it runs on pull_request to master, grants contents: read and pull-requests: write, checks out the repository with actions/checkout@v4, and runs actions/dependency-review-action@v4. Both action references are major-version tags rather than full commit SHAs, so the workflow does not pin the code that will execute. The pull-requests: write permission is explicitly needed by comment-summary-in-pr: always, and that gives the job authority to write PR comments. A compromised or retagged checkout or dependency-review action could run in this job, read the checked-out repository contents, and use the granted PR permission to post misleading dependency-review comments or suppress the useful review signal. GitHub fork-token restrictions can reduce exposure for some external fork PRs, but they do not mitigate same-repository PRs or the broader upstream action compromise scenario. The file has no local mitigation such as SHA pinning or splitting the comment-writing operation into a more constrained trusted job. Git history for this file only shows the workflow introduction commit, and there is no local diff on the target file, so the issue is still present.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
