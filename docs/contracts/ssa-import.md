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
- A position keeps the meaning its report gives it, as in the Python import:
  `desde` is the Tempo Excedido start in "Pendentes de Programar" and the
  Tempo Total start in "SSAs em Desvio" (the only `Desde` of that report).
- Per-event reports (parcial, desvio historico, espera) carry the event's
  responsible, description, times and justification under the SSA field
  aliases, so the retained event row writes those SSA fields, as in Python.
- Minimum schema is `numero_ssa`, `descricao_ssa`, and `data_cadastro`. SCC,
  ADI, and ASE may omit the date only when a valid `semana_cadastro` is
  present.
- SSA number normalizes spaces, `YYYY-XXXXX`, and Excel `.0`, then must contain
  exactly nine digits. Letters, ambiguity, and out-of-contract values reject
  the row.
- Excel 1900 and 1904 date systems are supported. A formula without cached
  value is rejected instead of being evaluated or accepted as empty. Date
  serials past 9999-12-31 reject the workbook.
- Row labels of per-event SAM reports are not counts: `Reprogramacao #N`
  (`num_reprogramacoes`) and `Desvio #N` (`numero_desvios`) yield no value, so
  the count from the reprogramming / `Numero de Desvios` count headers is kept. Legacy
  `Desvio #N` text becomes NULL when the SSA is updated.
- A continuation row of those reports (values, but no SSA number and no
  description) is not imported and counts as `skipped`; a blank row counts as
  nothing.
- `desde`, `desde_1`, `desde_2`, `ate`, `ate_1`, `ate_2` are stored as
  `YYYY-MM-DD HH:MM:SS` (column type `DateTimeText`) and shown as stored.
- The SAM "SSAs Legadas" report (`SAAs_*.xlsx`, alphanumeric numbers) is a
  known family that is ignored, like the derivation report.

## Snapshot And Merge Policy

Snapshot precedence follows the Python tuple `(data_planilha, file date)`:

1. spreadsheet date (`data_planilha`) decides first;
2. equal spreadsheet dates are broken by the file date: date and time encoded
   in the original source name, else source file modification time, else file
   creation time; a row without a file date sorts before any row with one, so
   the order is total and never depends on the order rows are read;
3. rows still equal are resolved by content, never rejected and never by read
   order: the row with more non-empty fields wins, then the larger canonical
   JSON serialization (sorted keys, ASCII escapes, trimmed values, numbers as
   exact decimals, dates as `YYYY-MM-DD HH:MM:SS`, coded fields lowercased);
   identical rows are one; the loser only fills empty fields; the repeat is
   journaled at Info level;
4. registration date is the last fallback for a row with no other date; it
   never breaks a tie between rows that carry a spreadsheet or file date.

Value equality is canonical: the same date in another format, the same number
in another spelling (`1.0` and `1`, compared exactly, never as floating point)
and a case-only difference in a coded field are not changes; the stored
spelling is kept. This is the same rule as the Python importer.

Creation time comes last because a staged copy carries a fresh one; it is
transient metadata and never manufactures a persisted update by itself. A row
without any date never wins over a dated one and is not a conflict; a dated row,
or a tie without dates, updates an undated one. An older snapshot changes
nothing. An equal snapshot preserves `situacao` except that a terminal `STE` may
finalize a transient state, and may complete only empty fields or allowed
operational indicators. A newer snapshot updates only fields present and
non-empty in the input. Missing fields never erase stored values. `STE` (final
execution) and `SCA` (approved cancellation) are terminal; `SCS` remains
transient. A terminal record is never overwritten by a newer snapshot, not even
its indicators or metadata. Coded fields (`situacao`,
`setor_emissor`, `setor_executor`, `localizacao_codigo`,
`grau_prioridade_emissao`, `grau_prioridade_planejamento`, `execucao_simples`)
compare without case: a case-only difference is neither a change nor a conflict.

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

Equal snapshots and repeated rows:

- Filename timestamps have minute resolution; ISO names with a time
  (`2026-07-15T10.00`) use it. There is no extra modification-time tie-break:
  rows of the same instant are resolved by content (Snapshot precedence, item 3).
- The same rule covers an SSA repeated inside one workbook, across worksheets
  and across workbooks of the same instant: nothing is rejected, the winner is
  chosen by content, its `situacao` applies (a terminal promotion always wins),
  and the repeat is logged at Info as `Import repeated SSA, resolved by content`
  with SSA, first differing field, both values and both files.
- Numbers compare by exact decimal value (`1.0`, `1`, `1e0`), except codes with
  leading zeros or an explicit sign, which stay text (`007` differs from `7`).
- Known limits, pending an owner decision: rows are merged in read order
  against the session state, so a terminal row of the batch and a newer open row
  of the same SSA can end differently depending on order; a whole-run
  consolidation per SSA (as planned in the Python importer) would remove it.

## Atomicity And Results

- Incremental import commits each accepted file atomically through selective
  merge. Full rescan commits the complete accepted corpus in one transaction.
- A mixed or rejected full corpus preserves the complete previous database.
- Public workflow status includes `NoChanges`. Per-file status is `Applied`,
  `NoChanges`, `NoValidRows`, `Rejected`, `Failed`, or `Canceled`.
- `ImportSummary` reconciles discovered, accepted, rejected, pending,
  preserved, valid/invalid rows, inserts, updates, verified rows, unchanged
  rows, conflicts, consolidation, and no-survivor counts with the committed
  SQLite result. `verified` counts rows rewritten only to record a newer source
  (`arquivo_origem`, `data_arquivo_origem`, `data_criacao_arquivo`,
  `data_planilha`); `updates` counts business value changes.
- A failed incremental rescan publishes nothing and reports every file as not
  applied.
- Initial data setup with XLSX is all-or-nothing. It imports into a private
  area; a rejected workbook or a warning fails the setup and publishes neither
  the database nor any processed copy. The private input area lives inside
  `docs_entrada`, so the move stays on one volume. On success the processed
  copies move to `docs_entrada` before the database is published, never over
  an existing file (a taken name gets the next free one, `name__1`, as the
  import consolidation does), and a failed publication takes them back.
- Only committed `Applied`, `NoChanges`, and validated `NoValidRows` entries can
  enter the consolidation journal.
- Known limit: a journal row whose staged source and destination are both
  gone is ambiguous and stops every later import of that database until it is
  resolved by hand; the journal never releases a move on its own. Untagged
  staging copies (written before per-database tags) can be removed by the
  cleanup of another database sharing `docs_entrada`, which can leave such a
  row.
- A `docs_entrada` file whose identity, size or modification time changed
  between discovery and the consolidation plan (a sync client or a fixed-name
  export replacing it) fails the run: nothing is published and the file stays
  in `docs_entrada` for the next rescan.

## Cancellation, Scale, And Recovery

- Workbook XML is consumed incrementally in bounded chunks. The default is
  1,000 rows and a rescan CLI request may select 1 through 1,000 rows with
  `--import-chunk-rows`. Parser, mapper, copy, lock, SQLite, and consolidation
  observe cancellation between bounded units of work.
- A rescan CLI request may select a SQLite busy wait from 0 through 3,000 ms in
  5 ms steps with `--sqlite-busy-wait-ms`; the default is 3,000 ms. Both import
  controls are validated before staging or acquiring the import lock and are
  propagated through full, incremental, external, and recovery paths.
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
- Derivadas edges reject self-loops and conflicting parents, skip edges whose
  child SSA is absent from the database, deduplicate repeated edges, and
  preserve existing parents absent from a partial batch.
