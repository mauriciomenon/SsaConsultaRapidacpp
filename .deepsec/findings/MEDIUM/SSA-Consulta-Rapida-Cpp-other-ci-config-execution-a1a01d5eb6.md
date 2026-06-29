# [MEDIUM] PR-controlled ESLint configuration can influence trusted SARIF upload

**File:** [`.github/workflows/eslint.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/eslint.yml#L15-L49) (lines 15, 16, 17, 25, 27, 31, 34, 35, 36, 41, 42, 45, 49)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `other-ci-config-execution`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs on pull_request, checks out PR code, installs npm packages dynamically, and then executes npx eslint with --config .eslintrc.js from the checked-out tree. A PR can add or modify a JavaScript ESLint config that executes during linting and can generate or alter eslint-results.sarif before the later upload step writes results with security-events: write. This can poison code scanning output even though repository secrets are not explicitly passed to the shell step.

## Recommendation

Use an immutable ESLint config from the base branch or a non-executable JSON config for PR scans. Run untrusted linting with contents: read only, pin npm dependencies with a lockfile and npm ci --ignore-scripts where practical, and upload SARIF only from a trusted context.

## Revalidation

**Verdict:** true-positive

I read the complete ESLint workflow and confirmed it runs on push, pull_request, and schedule, grants security-events: write, checks out PR code, installs ESLint dynamically, runs npx eslint with --config .eslintrc.js, and then uploads eslint-results.sarif with upload-sarif@v3. The checkout step uses the PR workspace, so a pull request can add or modify .eslintrc.js and JavaScript or TypeScript files that ESLint will process. A JavaScript ESLint config is executable Node.js code, so the PR-controlled config can run during the lint step. Even if that code cannot directly read a write token from the shell environment, it can write or replace eslint-results.sarif in the workspace, and the later trusted upload action uses the job's security-events: write permission to publish it. The continue-on-error setting means the lint step can fail while still allowing the upload step to run, which makes prewriting a SARIF file a practical path. Fork PR token restrictions may limit some external cases, but same-repository PRs and repository settings that allow write tokens remain exploitable, and the workflow has no trust boundary between untrusted lint execution and trusted SARIF upload. No immutable base-branch config or non-executable config is used. The current file still contains this design and has no patch after the introduction commit.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
