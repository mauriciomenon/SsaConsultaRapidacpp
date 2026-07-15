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
  ADI, and ASE may omit the date only when a valid `semana_cadastro` is
  present.
- SSA number normalizes spaces, `YYYY-XXXXX`, and Excel `.0`, then must contain
  exactly nine digits. Letters, ambiguity, and out-of-contract values reject
  the row.
- Excel 1900 and 1904 date systems are supported. A formula without cached
  value is rejected instead of being evaluated or accepted as empty.

## Snapshot And Merge Policy

Snapshot precedence is:

1. date and time encoded in the original source name;
2. spreadsheet date;
3. file creation time when the filesystem exposes it;
4. source file modification time;
5. registration date, only as the last fallback.

The source filename is the primary evidence. Creation time is auxiliary metadata
and is currently transient because the SQLite schema is unchanged; it is not
used to manufacture a persisted update by itself. An unparseable comparison
fails closed. An older snapshot changes nothing. An equal snapshot preserves
`situacao` except that a terminal `STE` may finalize a transient state, and may
complete only empty fields or allowed operational indicators. A newer snapshot
updates only fields present and non-empty in the input. Missing fields never
erase stored values. `STE` (final execution) and `SCA` (approved cancellation)
are terminal; `SCS` remains transient. A terminal record does not change to a
different terminal or transient state, although newer indicators and snapshot
metadata may be recorded.

Source names are classified as `Executadas`, `DerivadasRelacionadas`, `Desvios`,
or `Geral`. Executadas is the strongest evidence for final STE rows. Derivadas
and related workbooks and files with `desvio` in the name are separate profiles;
their richer execution, delay, partial-run, waiting, deviation, scheduling,
and rescheduling fields may enrich an eligible snapshot without regressing the
status. This profile classification is domain metadata, not a second database
schema.

The current schema has one `descricao_execucao` column, so a valid incoming
execution overwrites that column as before. `ExecutionHistoryPolicy` already
selects `descricao_execucao_2`, `descricao_execucao_3`, and later columns when
they exist; adding those columns is deferred until schema stabilization. Sparse
workbooks preserve previously stored rich fields, while a richer equal snapshot
may fill or improve them.

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

## Related Explicit Workflows

- SAM refresh stages every sector artifact before opening the atomic SQLite
  write session. Each workbook schema and its manifest and physical row counts
  are validated inside the single transaction, and commit occurs only after
  every sector passes. A result at the configured 200-row limit is rejected as
  potentially truncated. The feature remains disabled by default.
- Derivadas import is separate from orphan cleanup. CSV, TXT, TSV, XLSX, and
  XLSM are supported directly. Legacy XLS is accepted only by explicit user
  selection after a visible LibreOffice availability preflight; it never
  participates in SSA discovery or rescan.
- Derivadas edges reject self-loops, conflicting parents, and missing children,
  deduplicate repeated edges, and preserve existing parents absent from a
  partial batch.
