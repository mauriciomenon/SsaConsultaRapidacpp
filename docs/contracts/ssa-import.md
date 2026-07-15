# SSA XLSX Import Contract

## Supported Input

- Main SSA import, incremental rescan, and full rescan accept only `.xlsx`.
- `.xls` is inventoried as pending but never parsed or converted by the SSA
  workflow. The legacy LibreOffice converter remains isolated for an explicit
  future workflow.
- Full discovery freezes root plus `processadas/`, excludes `nosurvivor/`, and
  processes each file sequentially without a corpus cap. The 64-file guard is
  limited to explicit external selection.
- Every worksheet is read in workbook order. A cover sheet may precede a data
  sheet; every data sheet must satisfy the same validation contract.

## Header And Row Validity

- The source contract lists 184 labels. Normalization collapses them into 168
  executable alias keys mapped to 77 canonical SSA fields.
- Repeated semantic families are resolved by documented position. An overflow,
  collision, or unresolved ambiguity rejects the file.
- Minimum schema is `numero_ssa`, `descricao_ssa`, and `data_cadastro`. SCC,
  ADI, and ASE may omit the date as defined by the domain policy.
- SSA number normalizes spaces, `YYYY-XXXXX`, and Excel `.0`, then must contain
  exactly nine digits. Letters, ambiguity, and out-of-contract values reject
  the row.
- Excel 1900 and 1904 date systems are supported. A formula without cached
  value is rejected instead of being evaluated or accepted as empty.

## Snapshot And Merge Policy

Snapshot precedence is:

1. spreadsheet date;
2. source file timestamp;
3. date in the original source name;
4. registration date.

An unparseable comparison fails closed. An older snapshot changes nothing. An
equal snapshot preserves `situacao` and may complete only empty fields or the
allowed operational indicators. A newer snapshot updates only fields present
and non-empty in the input. Missing fields never erase stored values, and STE
or SCA never regress.

## Atomicity And Results

- Incremental import commits each accepted file atomically through selective
  merge. Full rescan commits the complete accepted corpus in one transaction.
- A mixed or rejected full corpus preserves the complete previous database.
- Public workflow status includes `NoChanges`. Per-file status is `Applied`,
  `NoChanges`, `NoValidRows`, `Rejected`, `Failed`, or `Canceled`.
- `ImportSummary` reconciles discovered, accepted, rejected, pending,
  preserved, valid/invalid rows, inserts, updates, unchanged rows, conflicts,
  consolidation, and no-survivor counts with the committed SQLite result.
- Only committed `Applied`, `NoChanges`, and validated `NoValidRows` entries can
  enter the consolidation journal.

## Cancellation, Scale, And Recovery

- Workbook XML is consumed incrementally in chunks of 1,000 rows. Parser,
  mapper, copy, lock, SQLite, and consolidation observe cancellation between
  bounded units of work.
- Cancellation before commit rolls back. Cancellation after commit preserves
  success and reports an optional-stage warning.
- A cross-process lock serializes discovery and import decisions.
- The SQLite journal is committed with the primary mutation. Post-commit moves
  are idempotent and resume after restart without overwriting destinations.
- The 250,000-row regression fixture permits at most 256 MiB additional RSS.

## Deferred Workflows

- SAM fetch exists but SAM XLSX to SQLite is not validated until the explicit
  schema adapter and truncation proof pass end to end.
- Derivadas source import is separate from orphan cleanup and remains pending.
