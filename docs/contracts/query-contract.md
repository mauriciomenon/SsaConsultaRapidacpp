# Query Contract

## General Search

- Comma means implicit AND.
- Each term may match any configured general-search column.
- `!term` excludes rows matching the term.
- `^term` means starts with.
- `term$` means ends with.
- `$term` also means ends with and is kept as a compatibility prefix form.
- `=term` means exact match.
- Distinct value lists shown by the UI exclude SQL NULL and blank values.
- `~expr` means safe pattern match translated to SQL `LIKE` with escaping.
- Safe pattern uses case-insensitive full-field `LIKE` semantics. Use normal
  search for substring matching.
- Safe pattern supports literal characters and `.` as a single-character
  wildcard. Grouping, alternation, and repetition operators are rejected with
  an explicit SQL error instead of silently returning no rows.
- Empty safe patterns match every value. Normal UI parsing should drop empty filters before
  reaching SQL.
- SQLite schemas used for high-volume searches should provide `COLLATE NOCASE`
  indexes for frequently searched text columns. The repository does not create
  indexes silently because production databases may be opened read-only.
- Unsupported operators: `(`, `)`, `{`, `}`, `|`, `*`, `+`, and `?`.

## Column Filters

- Comma inside one column means OR for positive terms.
- Negative terms remove matches from the column result.
- Column filters do not create business aliases.
- Safe pattern filters use full-field `LIKE` semantics. Use normal column
  filters for substring matching.
- On a date column shown as `dd/mm/yyyy` (stored as ISO), a filter that
  contains a digit, `/` or `-`, and a digit matches each term against the
  stored value or the displayed date, as the Python GUI does (`21/12/2021`,
  `12/2021`, `!21/12/2021`).

## Status Shortcuts

- A status chip cycles its code through include (`=CODE`), exclude (`!=CODE`)
  and off. Exact terms (`=X`, `!=X`) read the same in a column filter and in an
  advanced filter; other column terms (`X`, `!X`, `^X`, `$X`, `~X`) do not.
- A column status filter with only exact terms moves to the advanced filter.
  One with any other term stays a column filter: the chips edit only its exact
  terms and keep the rest as typed.

## Reprogramming Counts

- `eq` matches any selected count; `lte` compares with the largest selected count and
  `gte` with the smallest, as the Python GUI does.
- A row without a count (`NULL`) stays out of every comparison, in C++ and in Python.

## Prohibited

- Fuzzy search.
- Business aliases.
- Hidden semantic normalization.
- Fallback to searching every technical field when GUI contract is known.
