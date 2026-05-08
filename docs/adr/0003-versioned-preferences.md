# ADR 0003: Versioned Preferences

## Decision

GUI preferences are treated as a versioned contract.

## Rationale

The Python GUI had drift between code defaults, examples, runtime files, and platform-specific
widths. The C++ GUI must make preference ownership explicit.

## Consequences

- Defaults live in code.
- Runtime preferences include a schema version.
- Platform-specific differences are allowed, but they must be explicit.
