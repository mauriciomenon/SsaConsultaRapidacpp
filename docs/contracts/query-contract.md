# Query Contract

## General Search

- Comma means implicit AND.
- Each term may match any configured general-search column.
- `!term` excludes rows matching the term.
- `^term` means starts with.
- `term$` or `$term` means ends with.
- `=term` means exact match.
- `~expr` means regex match through SQLite `REGEXP`.

## Column Filters

- Comma inside one column means OR for positive terms.
- Negative terms remove matches from the column result.
- Column filters do not create business aliases.

## Prohibited

- Fuzzy search.
- Business aliases.
- Hidden semantic normalization.
- Fallback to searching every technical field when GUI contract is known.
