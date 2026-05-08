# ADR 0005: GitHub Actions First

## Decision

The first CI implementation uses GitHub Actions.

## Rationale

The new repository starts clean and needs fast validation for CMake, Qt, tests, lint, and
packaging. CircleCI can be added later if the project needs it.

## Consequences

- macOS is the first production-like CI platform.
- Linux is used for portability checks.
- Windows remains planned after the first stable bundle.
