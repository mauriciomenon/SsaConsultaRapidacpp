# ADR 0001: Core Without Qt

## Decision

`domain`, `query`, and `ports` do not depend on Qt types.

## Rationale

The Python GUI accumulated business behavior inside GUI classes. Keeping the core free of
Qt prevents QML, QObject lifetime, and GUI state from becoming part of the business contract.

## Consequences

- Presentation adapters convert between Qt and domain types.
- Core tests run without a GUI runtime.
- Future CLI or service entrypoints can reuse query behavior.
