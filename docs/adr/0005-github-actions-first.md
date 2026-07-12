# ADR 0005: GitHub Actions First

## Status

Historical initial decision. GitLab CI is now the primary available CI and
Bitbucket Pipelines is the quota-limited mirror validation. GitHub Actions is
currently unavailable because the `gh` remote account returns HTTP 403. See
[`ROUND_STATUS.md`](../../ROUND_STATUS.md) for the dated operational state.

## Decision

The first CI implementation uses GitHub Actions.

## Rationale

The new repository starts clean and needs fast validation for CMake, Qt, tests, lint, and
packaging. CircleCI can be added later if the project needs it.

## Consequences

- macOS is the first production-like CI platform.
- Linux is used for portability checks.
- Windows remains planned after the first stable bundle.
