# ADR 0004: Pinned FetchContent Dependencies

## Decision

Test dependencies use CMake FetchContent with pinned tags.

## Rationale

This keeps bootstrap simple while avoiding floating dependency versions.

## Consequences

- CI and local builds use the same Catch2 version.
- Updating dependencies requires an explicit change to CMake.
