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

## Prohibited

- Fuzzy search.
- Business aliases.
- Hidden semantic normalization.
- Fallback to searching every technical field when GUI contract is known.
