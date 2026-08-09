# SAM and SSA Analytics Alignment Plan

## 1. Plan control

| Item | Value |
| --- | --- |
| Status | ACTIVE WORKING TREE, SOME DECISIONS CLOSED, NOT COMMITTED |
| Date | 2026-08-07 |
| Last decision update | 2026-08-09 |
| Branch at planning time | master |
| HEAD at planning time | 2c93f51c9e6033da943f16738cce54c043ac896b |
| Knowledge contract | [SAM and SSA Analytics Knowledge Base](../contracts/sam-ssa-analytics.md) |
| Production edits authorized by this document | None |
| Narrow edits authorized by the user | Primary/related-SSA visibility, SAM batch atomicity, real ISO-week validation, truthful labels, import-result accounting, and custom-report zero/table/CSV/ISO-month behavior |
| Commit/push policy in this round | Commit the validated slices on current master and publish to origin plus bitbucket; no branch, worktree, PR, or merge |
| Protected scanner baseline | .secrets.baseline must remain byte-for-byte unchanged |
| Timeout policy | Silence and timeout never select an option |

This file is the implementation plan. Business facts and evidence belong in the linked knowledge
base and are not duplicated here except where a slice needs an acceptance criterion.

## 2. Original request

The requested outcome is:

- understand the SSA import and analytics algorithms deeply;
- determine whether workbook order changes the database;
- explain large update counts without false positives;
- identify validation that is too loose or too strict;
- document exactly what the custom report assumes and omits;
- verify every GLM 5.2 point;
- discover new risks;
- preserve reusable SAM knowledge;
- keep the implementation plan separate;
- use small, testable, cross-platform slices;
- avoid overengineering and unrelated layout or cleanup changes.

## 3. Main and secondary goals

### 3.1 Main goal

Align stored SSA evidence, report formulas, analytics output, and UI wording so every exposed SAM
result is reproducible, provenance-aware, and faithful to an explicitly approved contract.

### 3.2 Secondary goals

- Make import results deterministic for equal-snapshot conflicts.
- Reject or visibly report malformed source values instead of erasing them.
- Make multi-sector SAM refresh atomic according to its documented contract.
- Separate unavailable, not applicable, unknown, and zero.
- Preserve current architecture boundaries and performance.
- Add missing tests before broad report implementation.
- Make the existing custom report auditable through exact zero handling, readable tables, and CSV.
- Keep optional Ponytail cleanup outside SAM commits.

## 4. Non-goals

- No full rewrite of import or analytics.
- No generic rules engine.
- No god service or all-report ViewModel.
- No loading the full SSA table into memory.
- No silent compatibility aliases without a documented source contract.
- No GUI layout or element-position change without separate approval.
- No branch, worktree, PR, merge, or rollback without dedicated authorization.
- No deletion of Ponytail candidates in this plan.
- No inferred answer for an open decision gate.

## 5. Baseline that every slice must preserve

The factual baseline is
[sam-ssa-analytics.md](../contracts/sam-ssa-analytics.md).

Critical current strengths:

- domain/query/ports remain free of Qt;
- QML has no SQL;
- parameter binding and catalog identifiers protect queries;
- incremental file savepoints and full-rescan rollback exist;
- cancellation and consolidation recovery are explicit;
- source filenames and snapshots have a documented precedence;
- analytics availability can represent an incomplete source;
- current focused tests are green at the planning HEAD.

Critical HEAD gaps and their current working-tree disposition:

- malformed primary or related SSA can disappear at HEAD; fixed and tested in the uncommitted working tree;
- equal-snapshot indicator conflicts are order-dependent;
- SAM sector artifacts are not batch-atomic at HEAD; fixed and tested in the uncommitted working
  tree;
- SAM status vocabulary conflicts with the domain/manual;
- real ISO-week validation is inconsistent at HEAD; fixed and tested in the uncommitted working
  tree;
- historical reports lack event evidence;
- two dashboard labels imply SAM formulas at HEAD; renamed without formula/layout change in the
  uncommitted working tree;
- dashboard history-period reuse, availability loading, and generic executed-history fallback
  remain open.

## 6. Decision gates

No slice may cross a gate with an empty Decision column.

| Gate | Question | Options to evaluate | Blocks | Decision |
| --- | --- | --- | --- | --- |
| D-01 | Urgent planning clock | 24 elapsed hours; 24 business hours; source-specific display | R05 temporal implementation | DECIDED 2026-08-09: 24 elapsed hours |
| D-02 | Business-time authority | Calendario do SOM/SAM named by G05; explicitly imported versioned calendar; another documented source | Business-time branches of R05, R06, R12 | OPEN; public location calendar was Codex research, not a user decision |
| D-03 | Plant business-clock offset | Fixed offset; named regional zone with daylight policy | All business clocks | DECIDED 2026-08-09: fixed UTC-03:00, no regional-zone or daylight adjustment |
| D-04 | Two files disagree about one SSA at the same update time: which value wins? | File processed later; named source priority; more complete row by explicit fields; reject as visible conflict | Import determinism behavior | OPEN |
| D-05 | R01 operation discriminator | Existing canonical field; derived organization rule; new source field | R01 | OPEN |
| D-06 | R08 population | SEE only; active execution set; another documented set | R08 | OPEN |
| D-07 | R10 terminal membership | STE only; STE plus SES | R10 | OPEN |
| D-08 | SAM status vocabulary | ALE/ASL; AIP/ASI aliases; endpoint-version mapping | SAM adapter | OPEN |
| D-09 | SCC in SAM artifacts | Accept and map; reject by endpoint contract | SAM adapter | OPEN |
| D-10 | R11 expanded traversal | Define direction per relation category, direct/transitive depth, cycle and missing-node behavior, and paging/result limit | R11 expanded traversal | OPEN |
| D-11 | N/A versus unknown presentation | Separate series; exclude unknown only; explicit quality panel | R09 and shared UI | OPEN |
| D-12 | Historical event authority | SAM event API; versioned snapshots; dedicated import | R02, R03, R06, R07, R08 latest partial, R10 counts, R12 | OPEN |
| D-13 | Analytics product naming | Generic operational dashboard plus SAM reports; replace dashboard; clearly separate both | UI exposure | OPEN |
| D-14 | Exact 50 percent and zero boundaries | Select the adjacent color for each boundary | Shared color boundary | DECIDED 2026-08-09: 50 percent green; zero yellow |
| D-15 | Emission approval sequence | G05 sector-then-division; another explicitly documented level sequence | R06 | OPEN |
| D-16 | Simple-execution deadline | Reconcile G05 Event 32 at 48 hours with the requested programmed-week deadline; define discriminator, start, and clock | R08 and R09 | OPEN |

## 7. Slice strategy

Each slice is independently buildable, testable, reviewable, and revertible. A slice may be split
further if its predicted diff grows beyond one coherent behavior.

### Slice S0 - Freeze knowledge and acceptance vocabulary

Status:

- Knowledge and plan documents updated in the working tree through 2026-08-07.
- Every business conflict and decision gate remains OPEN.
- Source behavior changed only in the later approved narrow slices recorded below.
- Neither document is committed at HEAD.

Files:

- docs/contracts/sam-ssa-analytics.md
- docs/plans/2026-08-06-sam-ssa-analytics-alignment.md

Acceptance:

- report count is R01-R12 plus one transverse time/color rule;
- every conflict is OPEN;
- GLM claims have corrected classifications;
- Ponytail is isolated as annotations only;
- no local absolute attachment paths or secrets are committed.

Rollback:

- Revert only the documentation commit if later rejected.

### Slice S1 - Add reproductions before behavior changes

Goal:

- Turn the highest-risk audit claims into executable, failing-before-fix tests.

Status on 2026-08-07:

- malformed related-SSA reproduction and regression coverage: COMPLETE IN WORKING TREE;
- SAM valid-sibling plus later operational failure rollback test: COMPLETE IN WORKING TREE;
- 2020-W53/2021-W53 domain, import, SAM, and advanced-filter coverage: COMPLETE IN WORKING TREE;
- equal-snapshot reverse-order permanent test: DEFERRED until D-04 defines the expected winner;
- ALE/ASL/AIP/ASI/SCC behavior: source-characterized, but no desired assertion while D-08/D-09
  remain open;
- canonical and legacy !STE behavior: reverified without selecting a new semantic rule.

Predicted files:

- tests/unit/SsaImportPolicyTests.cpp
- tests/integration/SpreadsheetImportWorkflowPortTests.cpp
- tests/unit/ActivityAnalyticsDomainTests.cpp
- tests/unit/SqlQueryBuilderTests.cpp
- possibly a focused SamSpreadsheetAdapter test file if existing ownership is too broad

Required tests:

1. Import the same equal-snapshot rows in A,B and B,A order with different indicator values and
   compare final persisted rows.
2. Import malformed numero_ssa_relacionada and assert an explicit invalid-row or rejected-file
   result, never silent omission.
3. Submit one valid SAM sector followed by a sector that reaches the incremental
   operational-error or duplicate-conflict branch; assert no sector commits. A schema/manifest
   rejection alone is not a reproduction because it already rolls back the session.
4. Check 202053 valid and 202153 invalid in SAM, import, analytics, and advanced exact filters.
5. Check ALE, ASL, AIP, ASI, and SCC behavior without deciding D-08/D-09 in the assertion; first
   capture current behavior as characterization tests.
6. Exercise legacy raw !STE and canonical !=STE display/execution behavior.

Constraint:

- Characterization tests must label current incorrect behavior clearly and must not become a
  permanent desired contract by accident.

Performance:

- Fixtures remain minimal; no full corpus in unit tests.

Commit:

- One atomic test-only commit.

Rollback:

- Revert the test commit without source changes.

### Slice S2 - Stop silent related-SSA loss

Goal:

- Preserve the invalid source value long enough to produce an explicit diagnostic.

Status on 2026-08-07:

- IMPLEMENTED AND VALIDATED IN WORKING TREE; NOT COMMITTED.
- The mapper keeps trimmed malformed nonempty text so the existing writer validation rejects it.
- Empty optional and valid canonical references keep their previous behavior.
- Read-only corpus inventory found zero malformed recognized edges in 174 unique specialized
  exports, but the canonical 1,355-file area had no generic related header and cannot establish
  prevalence.

Smallest expected change:

- Validate related SSA text before the mapper omits empty normalized values.
- Reuse SsaImportPolicy::normalizeNumber; do not introduce a generic validation framework.
- Report the row/file through the existing invalid or rejected result model.

Predicted files:

- src/infra/import/SsaSpreadsheetMapper.cpp
- its narrow result type only if the existing invalid counters cannot express the error;
- focused mapper/workflow tests.

Acceptance:

- valid formatted and Excel-suffixed references still canonicalize;
- empty optional reference remains allowed;
- malformed nonempty reference is visible;
- no partial file commit;
- import summary reconciles all rows.

Risk:

- Existing workbooks containing malformed optional references may begin rejecting rows/files.
- Before enabling, run a read-only corpus inventory and report the count and filenames.

Performance:

- One normalization/validation per populated related field; no extra scan.

Commit:

- One behavior plus tests commit.

Rollback:

- Revert the commit; no schema migration.

### Slice S3 - Make equal-snapshot handling deterministic

Gate:

- D-04 must be decided.

Goal:

- The same corpus produces the same database regardless of explicit file-selection order.

Preferred minimal shape after D-04:

- Canonicalize the complete explicit selection before its 64-file partition, then preserve that
  order inside every staged batch.
- Make the merge winner explicit for different equal-snapshot indicator values.
- Preserve the existing no-change comparison for identical values.
- Do not add a public CLI import-order switch unless a real user workflow requires it.

Why no CLI switch by default:

- Exposing stable versus caller order preserves two behaviors and increases the test matrix.
- The product needs one deterministic contract, not a permanent debugging option.

Predicted files:

- src/infra/import/ImportFileStager.cpp
- src/infra/import/SpreadsheetImportWorkflowPort.cpp
- src/domain/SsaImportPolicy.cpp
- focused unit/integration tests
- docs/contracts/ssa-import.md after behavior is proven

Acceptance:

- A,B and B,A fixtures produce byte-equivalent selected row values;
- equivalent selections larger than 64 files preserve the same global batch order;
- older/newer snapshot behavior is unchanged;
- terminal promotion is unchanged;
- equal same-value import reports unchanged;
- conflicts and file atomicity remain explicit;
- corpus result counts are explained before and after.

Performance:

- Sorting remains O(n log n) over files.
- Merge stays one pass over incoming fields.

Commit:

- Separate deterministic staging and merge-policy commits only if each is independently valid.

Rollback:

- Revert behavior commit; no schema migration.

### Slice S4 - Restore SAM refresh contract

Gates:

- Atomicity and real ISO-week validation have no business gate.
- D-08 and D-09 block only the status-vocabulary sub-slice.

Status on 2026-08-07:

- batch atomicity: IMPLEMENTED AND VALIDATED IN WORKING TREE; NOT COMMITTED;
- real ISO-week validation: IMPLEMENTED AND VALIDATED IN WORKING TREE; NOT COMMITTED;
- SAM status vocabulary and SCC: NOT CHANGED; D-08/D-09 remain OPEN;
- the portable Windows test covers a later operational-error rollback, while the older
  POSIX-guarded end-to-end block remains a cross-platform coverage gap.

Goals:

- Treat all sectors in one refresh as one commit decision.
- Use one authoritative status vocabulary.
- Reject impossible ISO weeks.

Minimal atomicity approach:

- Keep incremental SSA imports per-file.
- In SpreadsheetImportWorkflowPort, treat samArtifacts not null as whole-batch failure policy.
- On any SAM artifact rejection or operational failure, roll back the write session.
- Do not misuse replaceAll, because SAM refresh must not clear unrelated SSA rows.

Status approach:

- Reuse the canonical domain status catalog.
- If endpoint aliases are required, map them in one explicit SAM-boundary table with tests and
  documented endpoint evidence.
- Never persist two aliases for one business state.

Week approach:

- Move real ISO year/week validation into one small domain primitive usable by import, SAM,
  analytics, and query code.
- Avoid coupling domain to Qt.

Predicted files:

- src/infra/import/SpreadsheetImportWorkflowPort.cpp
- src/infra/import/SamSpreadsheetAdapter.cpp
- src/domain status/week ownership files
- tests/integration/SpreadsheetImportWorkflowPortTests.cpp
- unit tests for adapter/domain
- docs/contracts/ssa-import.md

Acceptance:

- one invalid sector leaves every sector and existing DB unchanged;
- all valid sectors commit together;
- accepted/rejected counters reconcile;
- ALE/ASL/aliases follow the approved gate;
- 202153 rejects and 202053 accepts;
- Windows runs SAM integration tests or gains an equivalent portable suite;
- staged cleanup diagnostics remain visible.

Performance:

- No second workbook parse.
- No full-table replacement.

Commit:

- Atomicity, status vocabulary, and week validation should be separate commits if each remains
  independently stable.

Rollback:

- No migration; revert the affected commit.

### Slice S5 - Investigate SAM artifact identity

Goal:

- Reproduce or falsify the suspected gap between SAM manifest evidence and the exact staged bytes.

Status on 2026-08-07:

- Broad generic-import TOCTOU allegation: REFUTED by existing identity/size/mtime checks around
  CancelableFileCopy and parsing of the staged copy.
- Narrow manifest-to-artifact byte binding: OPEN, not reproduced.
- Original-path identity after staging and before consolidation: OPEN as a narrow lifecycle risk.
- Source change: NONE; no blanket hash or wrapper was added.

Keep it simple:

- The current flow already copies to staging and parses the staged file.
- Start with a controlled swap/race test around the SAM artifact and manifest boundary.
- Inventory what size, modification, copy, and source checks already prove.
- Do not hash every ordinary XLSX import without a reproduced gap and measured benefit.

Possible control only after reproduction:

- bind an upstream digest when the API supplies one for the exact bytes;
- otherwise add the cheapest identity check that closes the demonstrated window;
- use a staged-byte digest only if metadata/handle controls are insufficient.

Predicted files during investigation:

- tests around SAM refresh and staging;
- SAM artifact type or staging code only after the reproduction proves a missing invariant.

Acceptance:

- the test states the exact byte/manifest invariant;
- the current implementation either passes, closing the finding, or fails reproducibly;
- any proposed control has cancellation and representative-file cost measurements;
- ordinary imports remain unchanged unless separately proven vulnerable.

Commit:

- Test-only reproduction first. A control is a later atomic commit after evidence and review.

Rollback:

- Revert the test/control commit; no schema change is expected.

### Slice S6 - Make current analytics honest before adding reports

Gate:

- D-13 for product naming.

Status on 2026-08-07:

- Two misleading labels: FIXED AND VALIDATED IN WORKING TREE; NOT COMMITTED.
- Formula, denominator, layout, and selection behavior: unchanged.
- reportPeriod reused as historyPeriod: still CONFIRMED and OPEN.
- QML does not load availability before offering Partial attention: still CONFIRMED and OPEN.
- NotApplicableOrUnknown, technical fallback reasons, duplicate reason text, and executed-history
  fallback disclosure: still OPEN.

Goal:

- Prevent generic metrics from being mistaken for SAM formulas.

Expected small fixes:

- Use a true history period distinct from report period.
- Make Atrasadas por area filter only overdue or rename it to the actual all-class behavior.
- Rename count-composition percentage so it cannot be read as percent time remaining.
- Load availability before selection and disable/annotate unavailable metrics.
- Localize all unavailable reasons.
- Avoid duplicate reason text.
- Separate NotApplicable from Unknown in domain and output.

Predicted files:

- src/domain/ActivityAnalyticsTypes.h/.cpp
- src/application/ActivityAnalyticsChartModelBuilder.cpp
- src/presentation/ActivityAnalyticsViewModel.cpp
- app/desktop/qml/analytics/AnalyticsDashboard.qml
- app/desktop/qml/analytics/AnalyticsCustomAnalysis.qml
- app/desktop/qml/analytics/AnalyticsChartCard.qml
- focused unit, QtTest, and QML tests

Layout constraint:

- Text, state, and selection behavior only unless a separate layout approval is given.

Acceptance:

- title equals formula and denominator;
- quick preset result contains only what its title promises;
- unavailable metric is known before execution;
- not applicable and unknown have different serialized values and UI behavior;
- dashboard history covers the approved history period;
- no English technical fallback reaches PT-BR UI.

Performance:

- Availability loads once per source revision or explicit refresh.
- No additional full analytics capture.

Commit:

- One commit per coherent user-visible correction.

Rollback:

- Revert each UI/semantic commit; schema compatibility must be preserved until the N/A migration is
  independently proven.

### Slice S7 - Introduce the minimal temporal core

Gates:

- D-01, D-03, and D-14 are decided.
- D-02 remains OPEN only for business-time calendar loading; it does not block the elapsed-time
  calculator or the approved color boundaries.

Goal:

- Compute remaining, exceeded, total, and color with one tested domain contract.

Required domain values:

- instant with explicit timezone interpretation;
- clock kind;
- allowed interval or deadline;
- business calendar revision;
- result state: available, exceeded, not applicable, or unavailable;
- quality reason.

Keep it simple:

- One concrete calculator for elapsed time and one business-calendar boundary.
- No expression language.
- No generic workflow engine.
- No Qt in domain.
- No hidden fallback from business time to calendar days.

Likely ownership:

- src/domain for value types and pure calculations;
- src/ports for a small business-calendar source boundary;
- src/infra for the calendar adapter chosen by D-02; SQLite persistence only if that decision
  requires local versioned storage;
- query/application consume already explicit values.

Required tests:

- 100, above 50, exactly 50, below 50, exactly zero, and overdue;
- weekend and BR/PY holiday crossing after D-02 is decided;
- fixed UTC-03:00 boundary;
- missing calendar revision;
- negative or zero allowed interval;
- cancellation for any data-loading boundary.

Performance:

- Calendar lookup must be indexed/bounded.
- Bulk report computation must not call a remote service per row.

Commit:

- Domain calculator first, then calendar adapter, then integration.

Rollback:

- Revert adapters before domain primitives; do not leave a half-used schema.

### Slice S8 - Add provenance/event storage only where required

Gate:

- D-12.

Goal:

- Store enough immutable evidence for historical reports without duplicating the whole SSA table.

Reports requiring event evidence:

- R02/R03 deviations;
- R06 approval/rejection history;
- R07 partial executions;
- R08 latest partial state when current snapshots cannot prove event order;
- R10 partial and rescheduling counts when current snapshots cannot prove event order;
- R12 cancellation approval/rejections.

Design constraints:

- Append-only event identity and source revision.
- Unique constraint prevents duplicate ingestion.
- Keep current SSA snapshot table as current-state read model.
- Do not encode all events into one untyped JSON payload.
- Do not create one table per UI card.

Before schema edit:

- Inventory actual source event availability.
- Decide D-12.
- Create and validate a SQLite snapshot before migration.
- Back up CMake or other configuration with a timestamp only if that configuration is edited.
- Write migration and downgrade/recovery plan.

Acceptance:

- repeated source import is idempotent;
- event ordering is deterministic;
- source correction is represented explicitly, not silent overwrite;
- current snapshot can be derived or reconciled;
- historical report provenance points to event/source revision;
- migration preserves existing 96,479-row corpus copy and integrity checks.

Performance:

- Index by SSA, event kind, event time, and source identity only where queries prove need.
- Benchmark write amplification and report latency.

Commit:

- Schema/migration, importer, and report consumption as separate atomic commits.

Rollback:

- Restore pre-migration snapshot through documented SQLite backup procedure, never reset the
  repository or edit a live database in place.

### Slice S9 - Implement R01 through R04

Gates:

- D-05 for R01.
- D-08 and D-12 for deviation history only.

Order:

1. R01 pending programming.
2. R04 latest rescheduling from current fields if a source inventory confirms original, latest, and
   count provenance; R04 does not require complete event history.
3. R02 current deviation.
4. R03 deviation history after event evidence exists.

Acceptance per report:

- exact population fixture;
- formula boundary fixture;
- unavailable input fixture;
- provenance output;
- SQL does not load full table;
- port/application/ViewModel/QML integration;
- title and export use same definition.

Commit:

- One report per commit.

Rollback:

- Feature exposure can be reverted without deleting preserved event evidence.

### Slice S10 - Implement R05 through R07

Gates:

- D-01/D-02/D-03/D-12/D-14.
- D-15 before R06.

Order:

1. R05 urgent planning can use the decided 24 elapsed hours; its business-time branch waits for
   D-02.
2. R06 pending emission approval after approval levels and events are available.
3. R07 partial execution after event history is available.

Special acceptance:

- programavel and urgente never share the wrong clock;
- sector and division windows are separately observable;
- partial sequence and open/closed intervals are preserved;
- unavailable source is explicit, never zero.

Commit:

- One report per commit.

### Slice S11 - Implement R08 through R10

Gates:

- D-06, D-07, D-11, D-14, and D-16.
- D-12 for event-derived R08 latest-partial and R10 count measures.

Order:

1. R09 pending execution because SPG/current fields may support a bounded first version.
2. R08 in execution after its population is approved.
3. R10 executed after terminal membership is approved.

Special acceptance:

- programmed, planned, and executed weeks are never substituted silently;
- a fallback value changes availability/quality, not the canonical fact;
- partial execution uses the last partial only where the contract requires;
- not-applicable unplanned SSA is distinct from unknown data.
- simple execution uses only the approved discriminator and exact deadline from D-16.

Commit:

- One report per commit.

### Slice S12 - Implement R11 and R12

Gates:

- D-10 blocks only expanded R11 traversal; simplified direct-derived work can proceed first.
- D-02/D-03/D-12 for cancellation approval.

R11 approach:

- Reuse the existing graph port/model.
- Implement the simplified direct-derived contract first.
- After D-10, add the expanded direct/indirect derived-and-related contract as a separate sub-slice.
- Preserve edge category and direction.
- Use cycle detection and an approved result bound.
- Do not copy graph traversal into analytics SQL and presentation separately.

R12 approach:

- Consume cancellation request and approval events.
- Apply sector/division business clocks.
- Report rejection counts by level.

Commit:

- R11 and R12 are separate commits.

### Slice S13 - Visual and operational acceptance

Goal:

- Prove the result is readable, truthful, responsive, and portable.

Validation:

- offscreen QML runtime with geometry logs;
- screenshots at compact and wide window sizes;
- readable-glyph assertion or stable golden comparison in a controlled font environment;
- keyboard and screen-reader labels;
- export PNG/SVG contains the same labels and data;
- no popup/layout position changes outside explicit approval;
- cancellation/latest-wins behavior under rapid requests;
- representative database performance and memory;
- native Windows, Debian, and macOS evidence.

Commit:

- Tests and visual corrections should remain separate from report formulas when possible.

### Slice S14 - Custom report zero, table, CSV, and ISO reference month

Goal:

- Improve the existing generic custom report without inventing any OPEN SAM formula.

Status on 2026-08-09:

- IMPLEMENTED AND VALIDATED IN WORKING TREE; NOT YET COMMITTED.

Behavior:

- keep `null`/unknown distinct from numeric zero;
- optionally hide only categories whose every known series value is exactly zero and that contain
  no unknown or meaningful trend value;
- optionally list every known zero as Category, Series, Value even when its category is hidden;
- show a measured, scrollable table below the custom chart by default;
- export the main table and zero list as formula-safe UTF-8 CSV;
- write exports atomically and preserve exact CRLF bytes on Windows;
- derive ISO reference-month start/end from the weeks whose Thursdays belong to that month;
- keep calendar-month and ISO-reference-month navigation distinct.

Files:

- app/desktop/qml/analytics/AnalyticsChart.qml
- app/desktop/qml/analytics/AnalyticsChartCard.qml
- app/desktop/qml/analytics/AnalyticsChartTable.qml
- app/desktop/qml/analytics/AnalyticsCustomAnalysis.qml
- src/presentation/ActivityAnalyticsViewModel.cpp
- src/presentation/ActivityAnalyticsViewModel.h
- app/desktop/qml/analytics/tst_AnalyticsCharts.qml
- tests/smoke/ActivityAnalyticsViewModelTest.cpp
- tests/smoke/ActivityAnalyticsWindowQmlTest.cpp

Acceptance:

1. `[0, 0]` may be hidden when the option is on.
2. `[0, null]` remains visible and `null` remains `Sem dado`.
3. Zero rows are collected before hiding and can be exported independently.
4. A nonzero trend does not keep an observed all-zero category visible.
5. December 2020 maps to 2020-W49 through W53; January 2021 maps to W01 through W04.
6. CSV escapes fields and neutralizes spreadsheet formula leaders.
7. The writer atomically replaces an existing file and writes caller-provided CRLF unchanged.
8. The visual artifact shows chart, main table, zero table, and export actions at 1580x940.

Rollback:

- Revert this presentation/ViewModel/test slice without touching import or SAM business gates.

## 8. Validation matrix

| Layer | Required evidence |
| --- | --- |
| Domain | Boundary and invalid-value unit tests; no Qt |
| Import mapper | Valid, empty, malformed, duplicate, and alias fixtures |
| SQLite writer | Savepoint, rollback, deterministic merge, busy/cancel behavior |
| SAM adapter | Exact schema, manifest identity, status vocabulary, real ISO week |
| Query | Parameter order, population, date/week comparison, N/A versus unknown |
| Application | Availability and provenance propagated without fallback |
| ViewModel | Latest-wins, history period, localized reason, stable title |
| QML | Selector availability, preset truth, accessible labels, readable screenshot |
| Custom report | Exact-zero/null distinction, dynamic ISO boundaries, main/zero CSV, measured table, visual artifact |
| Performance | Representative corpus latency, RSS, write-lock duration, query plan |
| Security | Semgrep with timeout remediation, Gitleaks, detect-secrets nonmutating scan, TruffleHog |
| Windows | Native MSVC build, CTest, Qt/QML, package smoke |
| Debian | ext4 clone, canonical scripts, CTest/QML/package |
| macOS | Native arm64 build/test/package |
| External | origin and bitbucket refs/pipelines; review service distinguished from CI |

## 9. Required command policy

For any later source slice, follow the repository host scripts and run review before build:

1. Automatic code review immediately after editing.
2. clang-format dry-run and relevant static analysis.
3. Focused unit/integration tests.
4. Canonical host build.
5. Focused plus full CTest where risk justifies it.
6. QML lint/format and offscreen runtime for UI changes.
7. Security/secret scans.
8. Atomic commit only after local evidence.
9. Push to origin and bitbucket only with the required authorization and verify both refs.
10. Record external failures as external, never as passed.

## 10. Performance and concurrency acceptance

Every source slice must answer:

- How long is BEGIN IMMEDIATE held?
- Does parsing happen before or after the writer reservation?
- Is a full table scanned or sorted?
- Is work repeated per selected file?
- Can cancellation interrupt bounded units?
- Can a second process observe or publish partial results?
- Does latest-wins prevent stale UI publication?
- What is peak RSS on a representative corpus?
- Which index is used, proven by query plan where relevant?

No optimization is accepted solely from static intuition.

## 11. Security and failure handling

Required principles:

- errors remain visible at the functional boundary;
- no empty catch or silent fallback;
- source bytes are bound to manifest/provenance;
- SQL values remain parameterized;
- catalog controls identifiers;
- temporary artifacts have clear ownership and retry diagnostics;
- secrets never enter logs, docs, screenshots, or committed fixtures;
- scanner timeout or install failure is not a green result.

## 12. Ponytail disposition

The Ponytail candidates are recorded only in the knowledge document section
Ponytail annotations and concerns.

They are excluded from S0-S13 because:

- they do not fix a SAM acceptance criterion;
- removal would expand the validation surface;
- some are developer tools with possible manual consumers;
- combining deletion with semantic fixes would weaken rollback.

A future cleanup request must have its own goal, plan, approval, tests, and commits.

## 13. Delivery evidence template

Use this table after every slice:

| Requirement | Working tree | HEAD | Validated locally this round | External proof |
| --- | --- | --- | --- | --- |
| Behavior/test | Not started | Not committed | Not run | Not requested |

Never mark a dirty working-tree result as committed. Never describe a tool that was unavailable,
timed out, or rejected by authentication as successful validation.

Current-round evidence:

| Requirement | Working tree | HEAD | Validated locally this round | External proof |
| --- | --- | --- | --- | --- |
| Knowledge and separate reusable plan | Present | Not committed | ASCII, whitespace, structure, link, and content review | Primary manuals, supplied reports, official calendar/law sources |
| Malformed related SSA remains visible | Implemented | Absent at HEAD | Mapper/writer/workflow focused tests pass | Not applicable |
| Malformed primary SSA remains visible with empty description | Implemented | Absent at HEAD | Mapper invalid-row and full-rescan rollback regressions pass | Codex Security LOW/P3 finding remediated after sealed snapshot |
| SAM multi-sector operational failure is atomic | Implemented | Absent at HEAD | Portable rollback test passes; generic isolation regression passes | Not applicable |
| Real ISO week validation in domain, generic import, SAM, and filters | Implemented | Absent at HEAD | 2020-W53 accepted; 2021-W53 rejected; portable SAM regression passes | Qt QDate ISO rule |
| Two generic analytics labels match their actual count formulas | Implemented | Old labels at HEAD | all_qmllint, incremental build, and 3 QML offscreen tests pass | Not applicable |
| Import result matches rollback state | Implemented | Inflated/under-described result at HEAD | Focused SAM invalid-row and later-workbook rollback regressions pass | Not applicable |
| Custom report zero/table/CSV behavior | Implemented | Options and CSV absent at HEAD | Configured qmllint; focused 6/6; window suite 41/41; visual PNG inspected | Qt QSaveFile behavior |
| Dynamic ISO reference-month navigation | Implemented | ViewModel helper absent and fake used fixed W01-W04 at HEAD | December 2020 W49-W53 and January 2021 W01-W04 pass | Qt QDate ISO Thursday rule |
| D-01 through D-16 business choices | D-01, D-03, and D-14 decided; remaining gates preserved OPEN | No choice embedded | Decisions trace to the owner's 2026-08-09 response; timeout selected nothing | Additional sources do not decide D-02/D-10/D-16 |
| Current Windows development executable | Built from the uncommitted working tree | Not represented by HEAD alone | Clean MSVC amd64 build 575/575; full CTest 657/657 with 5 explicit skips; QML window 41/41 | No package or external service proof yet |
| Analytics report cardinality | Unbounded mechanism documented; no arbitrary cap added | Same unbounded contract at HEAD | Codex Security LOW/P3 source trace; no threshold benchmark exists | OPEN product decision based on measured legitimate cardinality |
| .secrets.baseline protection | Unchanged | Same bytes | Hash and git diff verification required at final gate | Not applicable |

## 14. Current to-do

| Order | Item | State | Blocker |
| --- | --- | --- | --- |
| 1 | Finish full diff review, scanners, and final status evidence | IN PROGRESS | Codex Security sealed 2 LOW/P3 findings; one fixed, cardinality OPEN; final scanners and CodeRabbit remain |
| 2 | Review and accept the two S0 documents | READY FOR USER REVIEW | User review |
| 3 | Complete only noncontroversial S1/S2/S4/S6 work authorized for this round | COMPLETE IN WORKING TREE | Commit remains separate |
| 4 | Resolve D-08/D-09 before SAM vocabulary edit | OPEN | Versioned business/API evidence |
| 5 | Resolve D-04 before deterministic winner edit | OPEN | Explicit decision |
| 6 | Implement elapsed-time/color core; resolve D-02 before business-calendar adapter | PARTIALLY UNBLOCKED | D-01/D-03/D-14 decided; D-02 remains OPEN |
| 7 | Resolve D-05/D-06/D-10 before R01/R08/R11 implementation | OPEN | Population/traversal contracts |
| 8 | Resolve D-12 before historical schema | OPEN | Event source authority |
| 9 | Resolve D-15/D-16 before approval/simple-execution formulas | OPEN | Business decisions |
| 10 | Keep Ponytail cleanup separate | RECORDED | Separate request |
| 11 | Commit and push validated slices on current master | PENDING FINAL GATE | Full clean build, package, scanners, and diff audit |

## 15. Residual risks

- The authoritative endpoint may use status aliases not documented in G05.
- A source event API may not expose enough history for R03/R07/R12.
- The malformed-reference path is fixed locally, but prevalence remains only partially sampled:
  zero malformed specialized edges and no generic related header in the canonical 1,355-file area.
- Business calendar revisions can change historical calculations if provenance is not stored.
- A visual golden test can be unstable unless fonts and rendering backend are controlled.
- Large scanners can time out on current C++ files and create a false sense of coverage.
- Custom-report arrays, visible delegates, width scans, and CSV are not bounded by a documented
  total-cell budget; keep this OPEN until representative cardinality and cross-platform memory/time
  measurements justify a product threshold.
- Cross-platform evidence remains incomplete until each native host runs its canonical scripts.

## 16. Completion definition

The alignment effort is complete only when:

1. All required decision gates have explicit recorded answers.
2. Each exposed report has a source, population, formula, clock, provenance, and unavailable rule.
3. Import order does not change the result under the approved contract.
4. SAM multi-sector refresh is all-or-nothing.
5. Invalid nonempty data cannot disappear silently.
6. N/A, unknown, unavailable, zero, and empty are distinguishable.
7. Labels and percentages describe the actual formula.
8. Domain, SQLite, application, ViewModel, and QML tests prove the same contract.
9. Performance and lock duration meet measured budgets.
10. Windows, Debian, and macOS evidence is recorded separately.
11. Security scanners complete or their gaps remain explicitly open.
12. The final commit and both required remote refs are verified.

Until then the plan remains DRAFT.
