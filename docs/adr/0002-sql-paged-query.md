# ADR 0002: SQL Paged Query

## Decision

The default GUI data flow queries SQLite by page.

## Rationale

The Python GUI filtered a global DataFrame in several paths. The real database currently has
more than eighty thousand rows and many columns, so loading everything for normal browsing
creates unnecessary memory pressure and complex cache behavior.

## Consequences

- Filters and sort are compiled to SQL.
- The table model only stores the visible page.
- Expensive caches are not introduced until a measured hotspot exists.
