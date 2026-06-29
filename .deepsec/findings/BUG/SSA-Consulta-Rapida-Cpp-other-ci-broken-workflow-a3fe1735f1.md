# [BUG] ESLint workflow references missing project config and may never upload SARIF

**File:** [`.github/workflows/eslint.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/eslint.yml#L34-L51) (lines 34, 35, 36, 41, 42, 45, 46, 49, 51)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** BUG  •  **Confidence:** high  •  **Slug:** `other-ci-broken-workflow`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The repository does not contain a tracked .eslintrc.js or package.json, but the workflow installs ESLint ad hoc and runs npx eslint with --config .eslintrc.js. The lint step has continue-on-error, but the upload step still expects eslint-results.sarif. If ESLint fails before producing that file, the SARIF upload fails and the scan workflow is effectively broken.

## Recommendation

Add the referenced ESLint config and package manifest/lockfile, or remove/replace this workflow. Ensure the SARIF file is created before upload, and fail clearly when configuration is missing.

## Revalidation

**Verdict:** true-positive

I checked the tracked repository files with hidden-file search and git ls-files and found no package.json, package-lock.json, npm-shrinkwrap.json, .eslintrc.*, or eslint.config.* in the tracked project. The workflow installs eslint@8.10.0 and @microsoft/eslint-formatter-sarif@3.1.0 ad hoc, then invokes npx eslint . --config .eslintrc.js and writes eslint-results.sarif. Because the referenced .eslintrc.js is absent on the current branch, ESLint will fail before producing the configured SARIF output in the normal push or scheduled workflow path. The lint step has continue-on-error: true, but that only lets the job continue; it does not create eslint-results.sarif. The next step unconditionally asks github/codeql-action/upload-sarif@v3 to upload eslint-results.sarif, so the workflow is likely to fail at upload when the file is missing. I found only an untracked .deepsec/package.json locally, which would not be present in a normal checkout and does not satisfy the workflow path. Git history shows only the workflow introduction commit and no later fix to add the config or manifest. This is a real broken-workflow bug, not a duplicate of the SARIF poisoning issue, because it describes the current base-branch failure mode.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
