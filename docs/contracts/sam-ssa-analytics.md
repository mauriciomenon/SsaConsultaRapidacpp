# SAM and SSA Analytics Knowledge Base

## 1. Document control

| Item | Value |
| --- | --- |
| Purpose | Reusable factual knowledge for SAM, SSA import, filters, and analytics |
| Status | VERIFIED BASELINE PLUS COMMITTED FIXES; PARTIAL OWNER DECISIONS; OPEN CONFLICTS |
| Last verified date | 2026-08-09 |
| Repository branch | master |
| Implementation delivery baseline | 152f2da2e70372b9b9a8e4ddf1a2ef9293c19493 on master |
| Evidence rule | Section 13.1 separates committed behavior, local validation, and external proof |
| Companion plan | [SAM and SSA analytics alignment plan](../plans/2026-08-06-sam-ssa-analytics-alignment.md) |
| Decision policy | A timeout, silence, or unanswered question is not a user decision |

This document records facts, evidence, limitations, and unresolved conflicts. It is not an
implementation sprint. Priorities, proposed changes, commit boundaries, and decision gates live
only in the companion plan.

The words CONFIRMED, PARTIAL, FALSE, STALE, UNSUPPORTED, CONFLICT, and UNKNOWN have precise
meanings:

- CONFIRMED: directly supported by the inspected source or executable evidence.
- PARTIAL: part of the claim is supported, but its consequence or scope was overstated.
- FALSE: contradicted by the inspected source or executable evidence.
- STALE: described an older state but not the inspected state named by the evidence.
- UNSUPPORTED: possible in principle, but the supplied evidence does not prove the attribution.
- CONFLICT: authoritative inputs disagree and no choice was made.
- UNKNOWN: the available sources do not define the required behavior.

## 2. Scope and exclusions

### 2.1 Included

- The twelve requested SAM/SSA report contracts.
- The shared time and color model used by those reports.
- SSA lifecycle, status, approval, planning, programming, execution, and cancellation rules.
- Current C++ import ordering, merge, validation, transaction, and consolidation behavior.
- SAM REST artifact import and its current validation boundaries.
- General, column, quick, advanced, and analytics filter behavior.
- Current activity analytics projection, SQL, application, ViewModel, and QML behavior.
- Custom-report zero handling, ISO-reference-month selection, textual table, and CSV export.
- Every material GLM 5.2 assertion in the supplied response.
- New findings discovered by direct inspection, tests, scanners, and independent agents.
- A dedicated Ponytail complexity audit record.

### 2.2 Excluded

- Choosing business semantics where the sources conflict.
- Treating the July 2026 SMIN report as a complete SAM formula specification.
- Treating the generator outage calendar as an SSA deadline authority.
- Implementing unresolved report formulas, schema migrations, or unrelated GUI layout changes.
- Treating any remaining OPEN business gate as decided by the committed technical fixes.
- Deleting optional tools based only on static reachability.
- Claiming Windows-only evidence proves Debian, macOS, or other architectures.

## 3. Source inventory and authority

The source PDFs and prompt attachments are not committed to this repository. Their SHA-256 values
make later re-identification possible.

| Source | SHA-256 | Role in this knowledge base |
| --- | --- | --- |
| Manual G01 - PT.pdf | 0F3A5D0C86B208B48F1A97638BE5D5007EB89FF20E39A897CEBD8F84D3FB38E8 | SOM, SAM, and SMA overview |
| Manual G02 - PT.pdf | C2C64578E0979ED53D3245B90B678BB6BFED7FA9D4A8AB913BDD16FD7BC56060 | Terminology |
| Manual G03 - PT.pdf | BEA31047440F9CA5204F999D633E8181C6DA09E49A4E4191835EB762E93A52C4 | Operational procedures and SOM calendar context |
| Manual G04 - PT.pdf | B799563EC79593331794DC96E8B1FEF6914EE716898E33593C00B437B5021877 | Equipment and location coding |
| Manual G05 - PT.pdf | ECCF6DBB3D9D5637787C7E2E50E886D226ED151764CA2AC9DF6BBA2E9C9D2F51 | Primary procedural authority for SSA |
| Manual G06 - PT.pdf | 1B169D15C2E0A5C9BA39F60A0D98F92E946B1AAE09E9E6A8FDABC23E3882CC5D | SAO and operation context, not report formulas |
| Analisis_SMIN_julio_2026.pdf | 1E9D6248483FB4ED8B31D9EB20B693DFE149AA355E014A92B4ABC1F9BFA501CD | Operational example of dashboard totals |
| Relatorio_CalendarioDeParadasDeUnidadesGeradoras_06_08_2026_15_42_29.pdf | 0203C7D3F785D3B4D688F44645F9ADCC56BF70A9ECF8883DDCA7EC880D1B7983 | PUG/outage context only |
| User report specification attachment | 33D8364047F6ED0257DC13D574517A53CE9EF97B75AA2FCB9D4BA32C4631F693 | Desired product behavior for R01-R12 and shared colors |
| GLM 5.2 assessment attachment | 048317C4DC4565BA79AB6A3A969C67080500D3FDCF39F0505B6BB55C79F8F06C | Claims to verify, not an authority |

### 3.1 Authority order

Use the following order when reading this document:

1. The explicit user specification defines the desired reports.
2. G05 defines documented SSA workflow and deadlines.
3. G01-G04 and G06 provide terminology and operational context.
4. The July 2026 SMIN analysis demonstrates a real reporting presentation and totals.
5. The current repository defines only what the product does now.
6. The GLM response is an audit hypothesis and must never override direct evidence.

A desired product requirement and a current manual rule can conflict. Such a conflict is recorded
instead of silently selecting one.

Additional primary sources checked by Codex on 2026-08-07:

- the official ITAIPU 2026 work calendar, Annex I DET/AD-AE/0006/2025 dated 2025-10-09;
- Paraguay Law 7544/2025, which defines national and movable holidays and permits up to three
  additional national holidays in special situations;
- Brazil Portaria MGI 11.460/2025, which defines the 2026 federal-administration calendar and
  distinguishes national, state, municipal, and optional observances;
- Qt QDate ISO-week documentation, used only to validate whether week 53 exists in a given year.

These sources improve data validation and calendar provenance. They were not supplied as a user
decision, do not choose a SAM business clock, and do not override G05. G05 identifies the
`Calendario do SOM`, registered in SAM, as its operational source of weekends and BR/PY holidays.

### 3.2 Known source conflicts

| ID | Subject | Evidence | State |
| --- | --- | --- | --- |
| C-01 | Urgent planning clock | G05 section 3.4.6 states a generic 24-hour threshold; section 3.9.11 and Event 18 say 24 elapsed hours; Table 12 PL-01 says 24 business hours | SOURCE CONFLICT PRESERVED; IMPLEMENTATION DECIDED 2026-08-09: 24 elapsed hours |
| C-02 | Emission approval wording | G05 says sector first, then division for a second level; the user specification summarizes one-level and two-level timing differently | CONFLICT |
| C-03 | SCS deadline text | G05 section 3.9.21 is headed SCS but its deadline sentence says SAS | CONFLICT, probable manual typo |
| C-04 | Operation-only discriminator in R01 | The requested report has an operation exception, but no authoritative field or classification rule was supplied | UNKNOWN |
| C-05 | R08 population | The requested formula defines in-time versus late execution, but does not fully define which lifecycle states enter the report | UNKNOWN |
| C-06 | SAM status vocabulary | G05 and ColumnCatalog use ALE and ASL; SamSpreadsheetAdapter uses AIP and ASI and omits SCC | CONFLICT |
| C-07 | Business calendar execution | G05 defines each eligible business day as 24 hours and says weekends and BR/PY holidays are registered in SAM through the Calendario do SOM. The additional public ITAIPU 2026 calendar was Codex research, not a selected application authority | CALENDAR SOURCE UNKNOWN; TIME OFFSET DECIDED 2026-08-09: fixed UTC-03:00 |
| C-08 | Simple-execution deadline | G05 Event 32 says simple execution expires after 48 hours; the report specification uses the end of the programmed week. Neither source defines the discriminator, start instant, or complete clock | CONFLICT and UNKNOWN |

The Evidence column records source facts. A dated implementation choice in State records an
explicit user decision and does not erase a conflicting source statement.

Calendar evidence does not resolve C-07. G05 points to the `Calendario do SOM` in SAM but does not
define an application API, export, revision identifier, or offline fallback. Annex I
DET/AD-AE/0006/2025 was additional Codex research. It has location-specific columns for ASU, CDE,
CHI-PY, AI, CHI-BR, FOZ, and BS, but no source says that the application must map each SSA to one
of those columns. That mapping was a Codex proposal and is withdrawn as a presumed requirement.
Paraguay Law 7544/2025 shows that movable and extraordinary holidays can change the annual set.
Recording the revision of any imported calendar is a Codex engineering recommendation, not a
rule selected by that law or by the user.

## 4. SSA domain model

### 4.1 Core concepts

| Concept | Meaning |
| --- | --- |
| SSA | Service request tracked through emission, planning, programming, execution, approval, or cancellation |
| Principal SSA | Request that can own dependent derived SSAs |
| Derived SSA | Dependency relationship whose prior execution is required by the principal relationship contract |
| Related SSA | Informational relationship such as recurrence, consequence, complement, equivalence, substitution, or impact |
| Programavel | Request whose service is not required within 24 hours under the G05 urgency definition |
| Urgente | Request expected within 24 hours because delay can affect energy, safety, assets, CHI integrity, or environment |
| Business time | Duration accumulated through eligible SOM calendar days; G05 says each eligible business day is a 24-hour period and excludes weekends and registered BR/PY holidays |
| Elapsed hour | Continuous wall-clock hour |
| Snapshot | Workbook view of an SSA at a source time; it is not an event history by itself |
| Event history | Ordered lifecycle facts preserving every transition or partial/reprogramming occurrence |

### 4.2 Relevant lifecycle states

| State | Meaning used by G05/current domain | Report relevance |
| --- | --- | --- |
| ASE | Awaiting sector approval during emission | R06 |
| ADI | Awaiting division approval during emission | R06 |
| APL | Awaiting planning | R05 |
| APG | Awaiting programming | R01, R02, R03 |
| AAD/AAT/ACC/ACS/ADM/AIM/ALE/AMP/APV/ASL/ASO | Waiting/deviation states documented by G05 | R02, R03, R07 |
| SPG | Programmed service | R09 and transition into execution |
| SRP | Service being reprogrammed | R04 |
| SEE | Service in execution | R08 |
| SAS | Awaiting sector approval during execution | Execution approval context |
| SAD | Awaiting division approval during execution | Execution approval context |
| STE | Finished service | R03 history and R10 |
| SES | Simple execution completed | Possible R10 membership, still UNKNOWN as a desired report rule |
| SCS | Cancellation awaiting sector approval | R12 |
| SCD | Cancellation awaiting division approval | R12 |
| SCA | Approved cancellation | Terminal cancellation |
| SCC | Canceled during registration | Emission cancellation context |

The state flow relevant to the reports is:

    registration -> ASE -> ADI -> APL -> APG -> SPG -> SEE -> SAS -> SAD -> STE
                                  |       |       |       |
                                  |       |       |       +-> waiting state -> partial/recovery
                                  |       |       +-> SRP -> SPG
                                  |       +-> waiting state -> APG
                                  +-> APG for operation paths without planning

    cancellation request -> SCS -> SCD -> SCA

Not every organization uses every approval or planning step. G05 explicitly says operation users do
not have the planning stage. That fact does not identify the missing R01 operation discriminator in
the current dataset.

### 4.3 Manual deadlines and transitions

| Rule | Documented value | G05 location |
| --- | --- | --- |
| Programavel emission approval by sector | 72 business hours | Page 27, section 3.3.3 |
| Programavel division approval after sector | Additional 72 business hours | Page 27, section 3.3.3 |
| Urgent emission approval | 2 elapsed hours | Page 27, section 3.3.3 |
| Programavel planning | 48 business hours | Pages 35 and 46 |
| Urgent planning | 24 elapsed hours selected for implementation; conflicting Table 12 text preserved | Owner decision 2026-08-09; G05 pages 35 and 46 |
| Programavel 1 service start target | Within 21 days from emission | Page 32 |
| Programavel 2 service start target | Within 60 days from emission | Page 33 |
| Programavel 3 service start target | More than 60 days from emission | Page 33 |
| Pending programming nonconformity | After two weeks as planned | Page 37, PR-01 |
| Partial execution edit window | Up to 30 days from latest partial execution | Page 40 |
| Simple-execution expiration | 48 hours; start and clock remain unspecified | Event 32 |
| Execution approval by sector/division | 72 business hours per applicable level | Pages 41 and 47 |
| Pending execution nonconformity | After two weeks as programmed | Page 43, EX-04 |
| Cancellation sector/division approval | 72 business hours in each applicable state | Pages 44 and 48 |

## 5. Shared time and color contract

### 5.1 Required primitives

Every time-sensitive report needs explicit values rather than a preformatted source string:

- start instant;
- deadline instant;
- observation instant;
- clock kind: elapsed, business, ISO-week comparison, or calendar-day display;
- calendar revision when business time is used;
- timezone;
- data provenance;
- whether the deadline is defined, exceeded, or not applicable.

For a report whose deadline is defined:

    total_elapsed = observation - start
    allowed = deadline - start using the report clock
    remaining = deadline - observation using the same clock
    remaining_ratio = remaining / allowed

Owner decisions recorded on 2026-08-09:

- urgent planning uses 24 elapsed hours;
- SAM business-clock timestamps use fixed `UTC-03:00`, with no Sao Paulo/Asuncion zone selection
  and no daylight-saving adjustment;
- exactly 50 percent remaining is green;
- exactly zero remaining is yellow, so overdue starts only when remaining time is negative.

The desired color interpretation is:

| Condition | Color | Meaning |
| --- | --- | --- |
| remaining_ratio greater than or equal to 0.50 | Green | At least half of allowed time remains |
| remaining_ratio less than 0.50 and greater than or equal to 0 | Yellow | Less than half remains, but deadline is not exceeded |
| remaining_ratio less than 0 | Red | Deadline exceeded |
| total elapsed | Blue | Time since the report-specific start |

The least restrictive adjacent color owns each exact boundary: 50 percent is green and zero is
yellow. Tests must cover both exact values and their neighbors.

### 5.2 Current implementation is not this contract

The current deadline projection uses SQLite julianday and casts the difference to an integer number
of calendar days in
[SqliteActivityAnalyticsProjection.cpp](../../src/infra/sqlite/SqliteActivityAnalyticsProjection.cpp#L267).
The SQL then classifies values using a configurable fixed warning window in days in
[ActivityAnalyticsSqlBuilder.cpp](../../src/query/ActivityAnalyticsSqlBuilder.cpp#L243).

The dashboard percentage is the composition of SSA counts by class, not the percentage of time
remaining. The denominator is summed counts in
[ActivityAnalyticsChartModelBuilder.cpp](../../src/application/ActivityAnalyticsChartModelBuilder.cpp#L519).

Therefore the current green/yellow/red appearance is visually similar but semantically divergent
from the requested SAM clock.

## 6. Data provenance and fitness

### 6.1 Current canonical analytics projection

The analytics fingerprint and stock projection include only eleven SSA source columns:

- numero_ssa;
- semana_cadastro;
- semana_executada;
- setor_executor;
- setor_emissor;
- situacao;
- solicitante;
- responsavel_programacao;
- responsavel_execucao;
- prazo_limite;
- status_execucao_prazo.

Evidence:
[SqliteActivityAnalyticsProjection.cpp](../../src/infra/sqlite/SqliteActivityAnalyticsProjection.cpp#L62).

This projection does not preserve priority, deviation history, rescheduling history, partial
execution history, approval levels, cancellation request time, or an authoritative business
calendar. Those reports cannot be reconstructed faithfully from the projection.

### 6.2 Available source fields versus report fitness

| Field family | Present in imported schema | Used by current analytics | Fitness |
| --- | --- | --- | --- |
| Emission/registration date and week | Yes | Yes | Event count only |
| Emission and planning priority | Yes | No | Raw snapshot value, not a clock |
| prazo_limite/status_execucao_prazo | Yes | Yes | Calendar-day classification only |
| tempo_disponivel/tempo_excedido/tempo_total | Yes | No | Imported text, provenance and formula not guaranteed |
| numero_desvios/situacao_de_desvio | Yes | No | Latest flattened value, not history |
| Reprogramming count and dates | Yes | No | Some latest values exist, event history absent |
| Partial execution/parciais/desde/ate | Yes | No | Flattened and variably formatted |
| Execution week and responsible person | Yes | Yes | Current execution event has fallback semantics |
| Derived and related identifiers | Yes | Outside analytics | Graph feature is separate |
| Approval level and rejection history | No complete canonical source | No | Insufficient |
| Cancellation request instant | No confirmed canonical source | No | Insufficient |
| BR/PY holiday calendar revision | No | No | Insufficient |

### 6.3 Provenance rule

A future result must carry at least:

- source type: live event, imported snapshot, or analytics snapshot;
- source file or API artifact identity;
- observed time;
- formula version;
- calendar revision where applicable;
- quality exclusions;
- unavailable reason when the result cannot be computed.

Missing data must not be converted to zero. Not applicable must not be merged with unknown.

## 7. Desired report contracts

The reports are R01 through R12. The shared time/color model is transverse and is not R13.

### 7.1 R01 - Pending programming

Purpose:

- Show SSAs waiting to be programmed.
- Show remaining, exceeded, and total time from emission to the planned deadline.
- For operation SSAs, show only elapsed time.

Expected population:

- APG. G05 defines programming as starting in APG and the SSA as programmed after it reaches SPG.

Required inputs:

- SSA number and current state;
- emission instant;
- planned deadline;
- sector/division;
- operation-versus-maintenance discriminator;
- observation instant.

Formula:

- allowed time is planned deadline minus emission;
- remaining and color use the shared ratio;
- exceeded begins after the planned deadline;
- total is elapsed since emission;
- operation exception suppresses remaining/exceeded calculations.

Open point:

- The operation discriminator is UNKNOWN.

Current repository:

- APG is counted as a stock metric, but no R01 clock or operation exception exists.
- G05 confirms that operation users bypass planning, but neither the inspected schema nor the
  supplied exports provide a versioned field that safely distinguishes every operation-only row.
- Classification as PRESENT based only on an APG count would be false.

### 7.2 R02 - Current programming deviation

Purpose:

- Show the latest deviation of each SSA and its deviation count.
- Include waiting states A-star except APG.

Required inputs:

- latest deviation start or confirmation instant;
- current waiting state;
- state-specific deadline;
- number of deviations;
- observation instant;
- justification and provenance.

Formula:

- allowed time is waiting-state deadline minus deviation start;
- overdue begins after that deadline; at the exact deadline the remaining-time color is yellow;
- total is elapsed from deviation start;
- expired deviation returns the SSA to APG according to the supplied report description.

Open points:

- The complete state/deadline table must be reconciled with G05.
- The status vocabulary conflict ALE/ASL versus AIP/ASI must be resolved.

Current repository:

- numero_desvios and situacao_de_desvio can be imported.
- No analytics aggregation or event ordering exists.

### 7.3 R03 - Programming deviation history

Purpose:

- Show every deviation through which an SSA passed.
- Include SSAs already in STE or another later state.

Required inputs:

- immutable deviation event identifier;
- SSA number;
- waiting state;
- start confirmation;
- completion or return instant;
- deadline and calendar revision;
- sequence number.

Formula:

- same remaining/exceeded model as R02 while the event is open;
- closed events show actual duration;
- history must not be reconstructed by overwriting a single current field.

Current repository:

- No authoritative deviation event table exists.
- A flattened snapshot cannot prove the full history.

### 7.4 R04 - SSAs with rescheduling

Purpose:

- Compare original programmed start with latest reprogrammed start.
- Show confirmation dates and total number of reschedulings.

Required inputs:

- data_inicio_programada;
- data_programacao;
- data_inicio_reprogramada;
- data_reprogramacao;
- num_reprogramacoes or an event count;
- provenance of the original and latest values.

Current repository:

- These columns are present in the import catalog.
- Current analytics does not read them.
- The source may hold only latest flattened values, so complete history is not guaranteed.

### 7.5 R05 - Pending planning

Purpose:

- Show APL SSAs with a countdown based on emission priority.

Required inputs:

- emission instant;
- emission priority;
- current APL state;
- applicable approval/planning path;
- business calendar or elapsed clock;
- observation instant.

Desired timing:

- Programavel: 48 business hours.
- Urgente: 24 elapsed hours, selected by the owner on 2026-08-09 and supported by the user
  specification and two G05 sections.
- G05 Table 12 still says 24 business hours; this source conflict remains recorded under C-01 but
  no longer leaves the implementation clock undecided.

Current repository:

- APL is counted.
- Priority is not projected into analytics.
- No business-time engine exists.

### 7.6 R06 - Pending emission approval

Purpose:

- Show ASE/ADI approval time, level, and rejection count.

Source-specific timing:

- G05: Programavel receives 72 business hours at sector level and another 72 business hours at
  division level after sector approval.
- User specification: one-level approval has 72 business hours; two-level approval has 72 business
  hours per division plus 72 business hours per sector, without an explicit sequence.
- Both sources use 2 elapsed hours for Urgente.
- C-02 must be decided before one canonical formula is implemented.

Required inputs:

- registration instant;
- priority;
- required approval levels;
- current approval state;
- sector and division approval/rejection events;
- business calendar revision;
- rejection count by level.

Current repository:

- Issued is an event volume based on registration week and emitter dimension.
- Issued does not mean approved and cannot proxy R06.
- Required level/rejection history is unavailable.

### 7.7 R07 - Pending with partial execution

Purpose:

- Show every partial execution, its waiting interval, deadline, percentage, exceeded time, and total
  time.

Required inputs:

- partial sequence;
- confirmation start;
- completion instant;
- waiting state and deadline;
- justification;
- observation instant for an open partial.

Current repository:

- The metric exists in the UI and query enum.
- The stock predicate is literal false, but the metric is explicitly marked incomplete with the
  reason complete partial-attention source is unavailable in
  [SqliteActivityAnalyticsProjection.cpp](../../src/infra/sqlite/SqliteActivityAnalyticsProjection.cpp#L42).
- This is EXPLICIT_UNAVAILABLE, not a silent zero report.

### 7.8 R08 - In execution

Purpose:

- Classify in-time versus late using execution week versus programmed start week.
- Show time from execution start to programmed finish.
- For partial execution, use the latest partial for the deadline classification.

Required inputs:

- exact population/state definition;
- execution start;
- programmed finish;
- execution week;
- programmed start week;
- latest partial identity when applicable.
- simple-execution discriminator and its programmed-week deadline.

Formula required by the user specification:

- within deadline when execution week equals programmed start week;
- outside deadline when those weeks differ;
- available time is programmed finish minus execution start;
- exceeded time starts after programmed finish, or after the programmed-week deadline for simple
  execution;
- total time is elapsed since execution start;
- partial execution uses the latest partial for its deadline classification.

Open point:

- The exact lifecycle population is C-05.
- The simple-execution clock and deadline conflict are C-08.

Current repository:

- The Executed metric uses terminal STE/SES membership.
- Its event week falls back from semana_executada to semana_programada and then semana_cadastro.
- That is not R08 and must not be mapped to it.

Read-only operational snapshot evidence from 2026-08-07 narrows, but does not close, C-05:

- 664 rows were in SEE;
- 661 had semana_programada;
- none had semana_executada;
- 93 had a usable programmed-start or confirmation date;
- 571 depended on the less specific desde field.

The counts support SEE as a candidate population. They do not prove that SEE is the complete
business set or define a single trustworthy start instant.

### 7.9 R09 - Pending execution

Purpose:

- Show programmed SSAs awaiting execution.
- Compare programmed start against planned date.
- Distinguish late, in time, and not applicable for unplanned SSAs.

Required inputs:

- programming confirmation instant;
- programmed start;
- planned date/deadline;
- whether planning applies;
- current state and observation instant.
- simple-execution discriminator and its programmed-week deadline.

Formula required by the user specification:

- outside deadline when programmed start exceeds the planned date;
- within deadline when programmed start does not exceed the planned date;
- not applicable when the SSA was not planned;
- available time is programmed start minus programming confirmation;
- exceeded time starts after programmed start, or after the simple-execution deadline;
- total time is elapsed since programming confirmation.

Current repository:

- SPG is counted as a stock metric.
- The date comparison and functional not-applicable result are absent.
- NotApplicableOrUnknown currently combines two meanings and is not sufficient.

### 7.10 R10 - Executed SSAs

Purpose:

- Show total reschedulings, partial execution count, latest execution responsible, and deadline
  classification by execution week versus programmed start week.

Required inputs:

- terminal membership rule;
- semana_executada;
- semana_programada or programmed start;
- rescheduling count;
- partial count;
- latest execution responsible.

Open point:

- Whether SES belongs with STE in the desired report is UNKNOWN.

Current repository:

- Responsible person and executed counts exist.
- Membership is STE/SES.
- Event week uses fallback values when execution week is absent.
- Rescheduling and partial counts are ignored.
- Classification is PARTIAL and DIVERGENT.

### 7.11 R11 - Derived and related SSAs

Purpose:

- Simplified view: direct derived SSAs for searched principals.
- Expanded view: direct and indirect derived and related associations.

Required inputs:

- directed derived edges;
- informational related edges and categories;
- traversal direction;
- cycle handling;
- depth/limit contract;
- missing-node behavior.

Manual rule:

- Related relationships are informational and have no deadline/business rule.

Current repository:

- Derivation and relation graph behavior exists outside activity analytics.
- It does not expose the requested simplified/expanded report modes as one analytics result.
- Within analytics this is ABSENT; at product level it is PARTIAL.

Corpus evidence from 2026-08-07:

- 174 content-unique Downloads exports whose names indicate derived/related data yielded 30,034
  recognized direct edges;
- no malformed edge, self-loop, multiple-parent file, or cycle was found in that export set;
- 9,285 repeated edge occurrences existed within files;
- every individual specialized export had depth one;
- an out-of-band auxiliary closure snapshot in the operational database contained reachable
  chains up to depth seven. It was last synchronized on 2026-07-28, while the database changed on
  2026-08-03, and the current C++ code neither owns nor queries those auxiliary tables.

The apparent disagreement is informative: the exports describe direct edges, while an out-of-band
process has composed edges across snapshots. Depth seven is not proof of current product behavior
or freshness. The evidence does not answer traversal direction per relation category, missing-node
behavior, one level versus transitive closure, bounded depth, or pagination. D-10 stays open.

### 7.12 R12 - Pending cancellation approval

Purpose:

- Show SCS/SCD cancellation approval timing and rejection counts.

Desired timing:

- 72 business hours for sector approval;
- another 72 business hours for division approval when applicable.

Required inputs:

- cancellation request instant;
- required approval levels;
- SCS/SCD transition events;
- business calendar revision;
- rejection count by level.

Current repository:

- No matching analytics report exists.
- G05 SCS deadline wording has conflict C-03.

### 7.13 Coverage summary

| Interpretation | Result at inspected HEAD |
| --- | --- |
| Strict activity analytics | 10 of 12 reports absent, R07 explicitly unavailable, R10 partial/divergent |
| Whole product | 9 absent, R07 explicitly unavailable, R10 partial/divergent, R11 partial outside analytics |
| Shared time/color rule | Divergent from current fixed calendar-day class composition |

The July 2026 SMIN PDF contains 45 registered, 43 STE executed, 335 partial attention, 37 SPG,
18 APG, 1 APL, 391 pending at month end, and 111 issued. These totals explain the current
dashboard vocabulary. They do not prove the twelve requested report formulas.

## 8. Current repository architecture

### 8.1 Layer ownership

| Layer | Current responsibility relevant here |
| --- | --- |
| src/domain | SSA types, import policy, filter tokens, analytics value types |
| src/query | General/column filters, advanced filters, analytics SQL generation |
| src/ports | Import, SAM refresh, analytics, and workflow boundaries |
| src/infra/import | Staging, workbook parsing/mapping, SAM adaptation, consolidation |
| src/infra/sqlite | Import transaction, schema, projection, analytics persistence |
| src/application | Workflow and analytics orchestration, chart model construction |
| src/presentation | Qt ViewModels and request lifecycle |
| app/desktop/qml | Selection, cards, charts, availability/error presentation |

No direct Qt dependency was found in domain/query/ports and no direct SQL was found in QML during
this audit. The broad architecture separation is a current strength.

### 8.2 Import pipeline

The main flow is:

    discover/select
      -> stage/copy
      -> read workbook
      -> map headers and rows
      -> resolve duplicate SSA numbers
      -> start SQLite write session
      -> per-file savepoint
      -> merge existing and incoming rows
      -> update analytics in the write transaction
      -> commit database
      -> consolidate source files with journal recovery

Primary evidence:

- [ImportFileStager.cpp](../../src/infra/import/ImportFileStager.cpp#L201)
- [SsaSpreadsheetMapper.cpp](../../src/infra/import/SsaSpreadsheetMapper.cpp#L383)
- [SsaImportPolicy.cpp](../../src/domain/SsaImportPolicy.cpp#L622)
- [SqliteSsaImportWriter.cpp](../../src/infra/sqlite/SqliteSsaImportWriter.cpp#L931)
- [SpreadsheetImportWorkflowPort.cpp](../../src/infra/import/SpreadsheetImportWorkflowPort.cpp#L1269)

The generic stager does not merely trust a path after discovery. CancelableFileCopy snapshots and
rechecks source identity, size, and modification metadata around the copy. This refutes the broad
claim that every generic import has an unguarded discovery-to-read swap. The narrower SAM gap is
different: manifest counts and fields do not cryptographically bind the manifest decision to the
exact artifact bytes later parsed. No blanket hashing change is justified without a controlled
reproduction and cost measurement.

### 8.3 Snapshot precedence

The current merge snapshot key is:

1. timestamp parsed from arquivo_origem;
2. data_planilha;
3. data_criacao_arquivo;
4. data_arquivo_origem;
5. data_cadastro.

Evidence:
[SsaImportPolicy.cpp](../../src/domain/SsaImportPolicy.cpp#L343).

When either existing or incoming has no valid snapshot, the existing row wins and a difference is
reported as conflict. An older snapshot is ignored except for terminal promotion. An equal snapshot
uses three different rules:

- metadata may choose a lexicographically smaller value;
- conflict fields may set conflict unless richer/preferred source rules apply;
- indicator fields can overwrite with the incoming value.

Because explicit external selection preserves caller order while discovery sorts workbookPath,
equal-snapshot indicator values can produce different final data under different file order.
Evidence:

- [ImportFileStager.cpp](../../src/infra/import/ImportFileStager.cpp#L201)
- [ImportFileStager.cpp](../../src/infra/import/ImportFileStager.cpp#L601)
- [SsaImportPolicy.cpp](../../src/domain/SsaImportPolicy.cpp#L643)

Plain-language version: two files can describe the same SSA at the same update time but contain
different indicator values. Current code keeps the value from whichever file is processed later.
Changing file order can therefore change the database. D-04 asks whether to keep that rule, always
trust one named source, choose the more complete row by an explicit field list, or reject the pair
as a visible conflict. No choice has been made.

Important qualification: assigning an indicator to the same stored value does not set changed,
because differsInPersistedValues compares the final map with existing values. The GLM proposal to
avoid identical-value writes describes behavior already present.

### 8.4 Meaning of the large update count

The recorded full-corpus run had:

- 1,692 files;
- 458,864 valid logical rows;
- 96,479 inserted final records;
- 362,385 updates;
- 1,241,540 unchanged rows;
- 1,603,953 duplicates;
- zero conflicts and zero invalid rows.

The equality 96,479 plus 362,385 equals 458,864. That is consistent with later snapshots updating
records inserted earlier in the same ordered corpus. It is not proof that normalization rewrote the
whole table.

The inspected writer has a dirty normalization ledger and compares final persisted values. A
noncanonical existing row encountered by an incoming SSA can still be normalized, but the GLM
attribution of the entire 362,385 count to normalization or identical indicator writes is
unsupported.

Historical evidence:
[ROUND_STATUS.md](../../ROUND_STATUS.md#L746).

### 8.5 Conflict return and atomicity

[SqliteSsaImportWriter.cpp](../../src/infra/sqlite/SqliteSsaImportWriter.cpp#L962) returns from the
writer batch after the first merge conflict. This stops later diagnostic evaluation within that
write call.

It is not proven persisted partial-row loss:

- incremental workbooks are wrapped in per-file savepoints;
- a duplicate conflict causes rollbackFile;
- full rescans roll back the entire replacement transaction.

Therefore the confirmed problem is incomplete evaluation/diagnostics and avoidable work, not the
GLM claim that later rows silently remain partially committed. Changing the behavior to skip only
the conflicting row would violate current file atomicity unless the product contract is changed.

### 8.6 Validation asymmetries

| Area | Behavior at HEAD 2c93f51 | Assessment on 2026-08-07 |
| --- | --- | --- |
| numero_desvios | Accepts integers, zero-fraction decimals, Desvio/Desvios prefixes, and known zero labels; 2 desvios becomes empty | PARTIAL GLM confirmation |
| num_reprogramacoes | Known labels normalize; unsupported text can reach integer binding and fail visibly | GLM silent-filter consequence is FALSE for normal import |
| Empty dates | Empty maps to absent/NULL | Intentional |
| Invalid dates | Any nonempty invalid date rejects the row | Strict and intentional |
| semana_cadastro with data_cadastro | Generic validation skips the week when a valid date exists | CONFIRMED at planning HEAD `2c93f51`; fixed in `ffdb594` |
| SAM year_week | Only six digits and week 1-53 are checked | CONFIRMED at planning HEAD `2c93f51`; fixed with real ISO validation in `ffdb594` |
| Advanced exact year/week | Year and 1-53 are composed without verifying whether week 53 exists | CONFIRMED at planning HEAD `2c93f51`; fixed in `ffdb594` |
| Related SSA | Mapper normalizes malformed value to empty and omits the key before writer validation | CONFIRMED at planning HEAD `2c93f51`; fixed in `ffdb594` by preserving malformed nonempty text for existing rejection |
| Todas as SSAs carve-out | Filename substring grants SCC/ADI/ASE incomplete-summary exemption | CONFIRMED broad source-name policy |
| Unknown numeric comparison mode | Normalizes to equals | CONFIRMED fail-closed/fail-open policy question |

The week fix uses one Qt-free C++20 domain primitive based on ISO Thursday/week-year rules. It
accepts 2020-W53 and rejects 2021-W53. It also makes a year-only advanced interval end at that
year's real last ISO week. No calendar-business rule was inferred from this technical validation.

### 8.7 SAM refresh path

The SAM path:

1. receives artifacts and manifest counts;
2. stages external files;
3. validates exact eleven-column schema;
4. validates manifest physical/detail counts, executor sector, number, date, week, and known status;
5. imports through the normal incremental per-file path.

Evidence:

- [SamSpreadsheetAdapter.cpp](../../src/infra/import/SamSpreadsheetAdapter.cpp#L21)
- [SamSpreadsheetAdapter.cpp](../../src/infra/import/SamSpreadsheetAdapter.cpp#L141)
- [SpreadsheetImportWorkflowPort.cpp](../../src/infra/import/SpreadsheetImportWorkflowPort.cpp#L984)

At HEAD, the contract says all sector artifacts pass before commit, but importSamArtifacts calls
importDiscoveredFiles with replaceAll false. A SAM schema/manifest rejection stored in
samRejectionReason already rolls back the whole session. The mismatch occurs when a sector reaches
an operational-error or duplicate-conflict branch handled by incremental isolateFileFailure:
successful siblings can still commit and the result can be Succeeded with a warning.

The working tree fixes only that policy mismatch: a batch with samArtifacts is atomic even though
replaceAll remains false, so unrelated SSA rows are not cleared. One portable Windows test imports
a valid SAM artifact followed by a corrupt workbook and proves the existing database is unchanged.
Generic incremental imports retain their per-file isolation behavior.

This fix is separate from staged artifact cleanup. Cleanup is an intentional temporary-artifact
lifecycle with visible diagnostics and service retry ownership, not independent proof of evidence
destruction. The older POSIX-only SAM integration block remains excluded on Windows; the new
portable regression test covers the atomic operational-error path, not every end-to-end SAM case.

### 8.8 Import result accounting after rollback

Two reporting defects were fixed in the working tree without changing row-selection or merge
policy:

- when an incremental file is isolated and rolled back, its inserts, updates, and unchanged rows
  are removed from the aggregate result before the per-file write counters are cleared;
- when a SAM batch is rejected before its first write, valid/invalid row counters are copied into
  the result and the diagnostic identifies the source filename.

The database transaction behavior remains the source of truth. A rolled-back write must never be
reported as persisted. Conflict and invalid counters remain visible because they explain why the
batch was rejected. Both corrections are constant-time accounting; they add no workbook scan,
query, transaction, or lock duration.

Evidence:

- [SpreadsheetImportWorkflowPort.cpp](../../src/infra/import/SpreadsheetImportWorkflowPort.cpp)
- [SpreadsheetImportWorkflowPortTests.cpp](../../tests/integration/SpreadsheetImportWorkflowPortTests.cpp)

## 9. Filter behavior

### 9.1 General and column text filters

The current parser behavior is:

| Input | General search | Column expression after current canonical UI serialization |
| --- | --- | --- |
| value | contains | equals when created by value selector |
| !value | not-contains | legacy raw value can remain not-contains |
| !=value | exact not-equal | exact not-equal |
| =value | exact equal | exact equal |

SearchParser first consumes ! as negation and then consumes = as exact mode:
[SearchParser.cpp](../../src/query/SearchParser.cpp#L9).

TextFilterToken treats both ! and != as the UI Different operator and serializes it as !=:
[TextFilterToken.cpp](../../src/domain/TextFilterToken.cpp#L11).

The SQL compiler uses the same negated LIKE/equality machinery for general and column terms:
[TextFilterSqlCompiler.cpp](../../src/query/TextFilterSqlCompiler.cpp#L134).

Conclusion:

- The GLM claim that current general !STE and column !STE inherently use different SQL semantics is
  FALSE.
- A persisted legacy raw !STE expression can be displayed as Different while still executing
  not-contains until it is edited or canonicalized. This residual presentation/execution drift is
  CONFIRMED.

### 9.2 Quick sector

Saved preferences migrate quickSector into an exact advanced executor token:
[FilterPreferencesNormalizer.cpp](../../src/presentation/FilterPreferencesNormalizer.cpp#L14).

A direct SsaPageRequest.quickSector still compiles with the default Contains match mode:
[TextFilterSqlCompiler.cpp](../../src/query/TextFilterSqlCompiler.cpp#L257).

The normal GUI path is exact after normalization. The GLM claim is STALE for ordinary persisted UI
use but remains a direct-port contract inconsistency.

### 9.3 Status exclusion

If an explicit status filter includes SCA, SES, or STE, the global exclusion toggle is turned off:
[FilterPreferencesNormalizer.cpp](../../src/presentation/FilterPreferencesNormalizer.cpp#L40).

This is intentional contradiction resolution, but the automatic state change should remain visible
and tested.

### 9.4 Distinct values

The current builder applies general, remaining column, and remaining advanced text predicates before
selecting distinct values:
[SqlQueryBuilder.cpp](../../src/query/SqlQueryBuilder.cpp#L368).

The self-filter is removed at the presentation request boundary. The GLM statement that no such
contract exists is STALE. Documentation historically lagged the implementation.

### 9.5 Sector and division

When both arrays reach analytics SQL, they are intersected with AND. That is a valid intersection,
not automatically a defect. QML clears selections that do not apply to the chosen breakdown.

The remaining gap is an integration test proving behavior for a deliberately mismatched direct
request.

## 10. Current activity analytics behavior

### 10.1 Metrics and cards

The selector exposes nine metrics:

1. Registered;
2. Executed;
3. Partial attention;
4. SPG;
5. APG;
6. APL;
7. Pending;
8. Issued;
9. Pending deadline.

The dashboard creates fourteen cards by presenting some metrics by sector and some as history. It
closely resembles the July SMIN dashboard, not the twelve desired SAM report set.

### 10.2 Event and stock semantics

- Registered and Issued use the same registration-week event; the difference is executor/emitter
  dimension.
- Executed membership is STE or SES.
- Executed event time falls back from execution week to programmed week and registration week.
- Pending means status not in SCA, SES, STE. Blank or unknown status is therefore pending.
- SPG/APG/APL are current stock counts.
- Partial attention is explicitly incomplete.
- Pending deadline is a current stock captured into analytics snapshots.

### 10.3 Presentation findings

| Severity | Finding | State and evidence |
| --- | --- | --- |
| HIGH | Dashboard request passes reportPeriod as both report and history period | CONFIRMED at HEAD and working tree: [ActivityAnalyticsViewModel.cpp](../../src/presentation/ActivityAnalyticsViewModel.cpp#L760) |
| HIGH | Atrasadas por area selects PendingDeadline but does not filter only overdue class | Label defect CONFIRMED at planning HEAD `2c93f51`; renamed to Prazo das pendentes por area in `3ff8ab1`, with formula unchanged: [AnalyticsCustomAnalysis.qml](../../app/desktop/qml/analytics/AnalyticsCustomAnalysis.qml#L744) |
| HIGH | Prazo das pendentes em percentual is count composition, not remaining-time percentage | Label defect CONFIRMED at planning HEAD `2c93f51`; renamed to Distribuicao percentual do prazo das pendentes in `3ff8ab1`, with denominator unchanged: [AnalyticsDashboard.qml](../../app/desktop/qml/analytics/AnalyticsDashboard.qml#L82) |
| HIGH | Availability exists in the ViewModel but QML does not load it before offering Partial attention | CONFIRMED at HEAD and working tree: the invokable exists, but no QML loadAvailability call was found: [ActivityAnalyticsViewModel.cpp](../../src/presentation/ActivityAnalyticsViewModel.cpp#L644) |
| HIGH | NotApplicableOrUnknown combines a valid functional result with data-quality failure | OPEN: [ActivityAnalyticsTypes.h](../../src/domain/ActivityAnalyticsTypes.h#L67) |
| MEDIUM | Colors resemble SAM but use fixed day classes | OPEN in current SQL/chart behavior |
| MEDIUM | Event and snapshot histories use similar visual language without per-point revision | OPEN in ViewModel/chart mapping |
| MEDIUM | Historico de executadas can use programmed or registration week when execution week is absent | OPEN: fallback is explicit in the projection, but the generic label does not disclose it |
| MEDIUM | Unknown unavailability reasons can fall through as English technical strings | OPEN in QML reason mapping |
| MEDIUM | Unavailability can appear in both quality note and body | OPEN in chart model plus card |
| LOW | Person-role title/control can appear when the selected breakdown does not use people | OPEN in current custom analysis UI |

### 10.4 Custom report zero, table, export, and ISO-month behavior

The working tree adds two independent presentation options to the custom report:

| Option | Default | Exact behavior |
| --- | --- | --- |
| Hide categories without occurrences | Off | Removes a category only when every series value is a finite numeric zero; any nonzero, `null`, missing value, or nonzero/missing trend preserves the category |
| List zero values | Off | Lists every known main-series cell whose value is exactly numeric zero as Category, Series, Value |

The options do not change the query, database, source model, totals, denominator, or sort contract.
The zero list is derived before category hiding, so it remains auditable when the main chart is
cleaned. `null` remains `Sem dado`; it is never coerced to zero. The list can report only
category/series combinations present in the returned model and does not invent absent people,
sectors, or source rows.

The custom chart now opens with its textual table below the plot. Column widths are measured from
the actual header and cell text, headers are bold, multiline categories use a readable separator,
and horizontal/vertical scrollbars preserve access to wide or long results. The table and the zero
list are independently exportable as CSV. CSV output is UTF-8 with BOM and CRLF, quotes embedded
commas/quotes/newlines, and prefixes spreadsheet-formula leaders before writing. `QSaveFile`
provides atomic replacement; binary-mode output preserves the already-normalized CRLF bytes.

The custom selector distinguishes calendar months from ISO reference months. In ISO mode, the
first boundary is the first ISO week whose Thursday is inside the selected month and the last
boundary is the last ISO week whose Thursday is inside that month. The ViewModel computes both
boundaries; QML does not use fixed week numbers. The UI displays the exact `YYYY-Www` start and end
and explains the Thursday rule. Verified boundaries include:

- December 2020: 2020-W49 through 2020-W53;
- January 2021: 2021-W01 through 2021-W04.

This is a technical period-definition improvement. It does not choose the OPEN D-02 business
calendar and does not alter the fixed UTC-03 decision for plant clocks.

Evidence:

- [AnalyticsChartCard.qml](../../app/desktop/qml/analytics/AnalyticsChartCard.qml)
- [AnalyticsChart.qml](../../app/desktop/qml/analytics/AnalyticsChart.qml)
- [AnalyticsChartTable.qml](../../app/desktop/qml/analytics/AnalyticsChartTable.qml)
- [AnalyticsCustomAnalysis.qml](../../app/desktop/qml/analytics/AnalyticsCustomAnalysis.qml)
- [ActivityAnalyticsViewModel.cpp](../../src/presentation/ActivityAnalyticsViewModel.cpp)
- [ActivityAnalyticsWindowQmlTest.cpp](../../tests/smoke/ActivityAnalyticsWindowQmlTest.cpp)

### 10.5 Screenshot evidence

The offscreen tests generated:

- build/windows/amd64/msvc/msvc2022_64/dev/activity-analytics-window-1180x760.png
- build/windows/amd64/msvc/msvc2022_64/dev/activity-analytics-window-1580x940.png
- build/windows/amd64/msvc/msvc2022_64/dev/activity-analytics-custom-report-1580x940.png

The inspected 1580x940 image rendered text glyphs as squares. An older runtime screenshot reportedly
rendered them correctly. This does not prove a source-code regression because the offscreen font
environment can differ, but it invalidates the current image as human visual evidence.

Current smoke tests assert non-null image and geometry, not golden pixels or readable glyphs.

The 2026-08-09 custom-report capture used the offscreen backend with the Windows font directory and
Qt Basic controls style. It visibly proves that the filtered chart, main table, zero list, and both
CSV actions fit at 1580x940. It remains runtime evidence, not a golden-pixel contract.

## 11. GLM 5.2 verification

### 11.1 Main conclusions

| GLM claim | Classification | Verified conclusion |
| --- | --- | --- |
| File order can change DB result | CONFIRMED | Equal-snapshot conflicting indicator values are last-writer-wins; external and rescan order differ |
| Identical indicator writes explain updates | FALSE | Final persisted comparison already avoids changed for the same value |
| Normalization explains 362,385 updates | UNSUPPORTED | Corpus arithmetic is consistent with later snapshots; dirty ledger limits normalization |
| Conflict return silently persists partial file | PARTIAL | Later rows are not evaluated, but file/full savepoints prevent the asserted partial persistence |
| Partial attention silently returns zero | FALSE | Predicate is false, but availability is explicitly incomplete and propagated |
| Current analytics implements 13 report categories | FALSE framing | There are 12 reports plus one transverse visual rule |
| Current report coverage is 9 absent, 1 stub, 2 divergent, 1 partial | PARTIAL | Strict mapping must distinguish R11 outside analytics and explicit unavailable state |
| Every historical backlog item remains open | STALE | Several named items were already corrected or closed at the inspected HEAD; only their reverified residuals belong in a new plan |

### 11.2 Validation claims A-N

| ID | GLM subject | Classification | Corrected result |
| --- | --- | --- | --- |
| A | numero_desvios | PARTIAL | Prefix and zero labels are supported; suffix form 2 desvios is omitted |
| B | num_reprogramacoes | PARTIAL/FALSE consequence | Unsupported text can fail integer binding visibly; normal import does not silently hide it only at filter time |
| C | dates empty versus invalid | CONFIRMED behavior | This is a deliberate NULL versus invalid-row distinction, not automatically inconsistent |
| D | semana_cadastro validation | CONFIRMED at planning HEAD `2c93f51` | Week could escape validation when data_cadastro existed; the generic, SAM, and advanced real-ISO gaps are fixed in `ffdb594` |
| E | !STE general versus column | FALSE as two current canonical semantics | Both canonical paths can be not-contains; only legacy raw persisted UI drift remains PARTIAL |
| F | quick sector equals versus contains | STALE/PARTIAL | Normal saved UI is migrated to equals; direct request still contains |
| G | Executed status membership | FALSE/STABLE | Executed is intentionally STE/SES and pending excludes terminal/canceled states |
| H | malformed related SSA | CONFIRMED at planning HEAD `2c93f51` | Mapper erased invalid text before writer validation; `ffdb594` preserves it for explicit rejection |
| I | Todas as SSAs carve-out | CONFIRMED | Filename substring grants special incomplete-row handling |
| J | unknown numeric mode | CONFIRMED | Falls back to equals |
| K | exclusion auto-off | CONFIRMED intentional | Resolves explicit-status contradiction |
| L | distinct self-filter missing | STALE | Implemented at presentation boundary; remaining filters reach SQL |
| M | sector plus division AND | CONFIRMED behavior | Correct intersection; direct mismatch test is missing |
| N | source filter bind order | STALE | Fixed at inspected HEAD |

### 11.3 Historical backlog claims

| Historical item | Current state |
| --- | --- |
| FILTER-DIFFERENT-SEMANTICS | Partial residual only for legacy raw persisted expression |
| DISTINCT-ADVANCED-TEXT-FILTERS | Resolved |
| DESKTOP-LOG-HANDLER-IO-LOCK | Resolved |
| IMPORT-WORKFLOW-HUB | Partially relevant; SAM contract mismatch remains |
| IMPORT-STAGER-HUB | Not a current functional defect |
| COLUMN-WIDTH-KEY-PAIRING | Resolved |
| ROTATING-LOG-UTF8-TRUNCATION | Resolved |
| SQLITE-IMPORT-NORMALIZE-FULL-SCAN | Resolved by dirty ledger for the broad claim |
| SQLITE-READ-CONNECTION-CHURN | Closed by prior benchmark/decision |
| SQL-DERIVADAS-BUILDER-BOUNDARY | Resolved |
| BROWSE-SECTOR-CAST-INVARIANT | Resolved |
| IMPORT-COMPLETENESS-LOOKUP | Resolved |
| IMPORT-FILENAME-TIMESTAMP-SCAN | Bounded cost, not shown as a functional bug |
| TEST-DETERMINISM-DELTA | Partial test debt outside the current report slice |

Historical claims must not be copied into a new plan without rechecking the current HEAD.

## 12. New findings

### 12.1 Data-integrity and determinism

| Severity | Finding at HEAD | Current disposition |
| --- | --- | --- |
| HIGH | Malformed related SSA is normalized to empty and omitted before writer validation | FIXED IN `ffdb594`: preserve malformed nonempty source text so existing writer validation rejects it visibly |
| LOW/P3 | Malformed primary SSA plus empty description can be mistaken for a continuation row before validation | FIXED IN `ffdb594`: preserve malformed nonempty primary text until the existing row validator counts and rejects it |
| HIGH | SAM multi-sector refresh can commit valid siblings after a sibling operational error or duplicate conflict | FIXED IN `ffdb594`: SAM batches use outer rollback; generic incremental file isolation is preserved |
| HIGH | SAM status vocabulary conflicts with the domain: AIP/ASI versus ALE/ASL, plus SCC omission | OPEN under D-08 and D-09; no alias or status was invented |
| HIGH | Equal-snapshot different indicator values depend on import order | OPEN under D-04; no winner was selected |
| MEDIUM | SAM manifest proof does not bind its decision to the exact artifact bytes later parsed | OPEN for controlled reproduction; metadata checks do not prove a manifest-to-byte binding |
| MEDIUM | SAM and generic import accept a syntactic week 53 in a year without ISO week 53 | FIXED IN `ffdb594`; 2020-W53 passes and 2021-W53 rejects |
| MEDIUM | Advanced exact and year-only filters can compose an impossible week 53 | FIXED IN `ffdb594`; year-only upper bound now uses the real final ISO week |
| LOW | Original-path identity can change after staging and before later consolidation actions | OPEN as a narrow lifecycle risk; no database parse reads the later original bytes |
| LOW | Canonical lock path can diverge from a later original path if a symlink parent is retargeted | OPEN theoretical path-lifecycle risk; no reproduced exploit or data error |

The broad generic-import TOCTOU allegation is REFUTED: CancelableFileCopy checks source identity,
size, and modification metadata around staging, and parsing uses the staged copy. The remaining
manifest-to-byte and post-staging original-path windows are narrower. They require controlled
reproduction before any digest, handle, or path-control change is selected.

Corpus qualification for the malformed-reference finding:

- the canonical operational import area had 1,355 XLSX files, but none exposed the generic related
  header needed to estimate this field's prevalence;
- the 174 content-unique specialized derived/related exports had zero malformed recognized edge
  rows after excluding their documented no-derived sentinel;
- absence in these samples does not make the silent-erasure path safe for a future generic source.

### 12.2 Performance

- The SQLite write session begins before workbook parsing finishes.
- BEGIN IMMEDIATE can therefore hold the single-writer reservation during parsing and analytics.
- Every commit computes a canonical fingerprint by sorted full projection scan.
- Analytics capture writes multiple aggregate families.
- Explicit external selection is capped at 64 files, but other workflows can repeat the cost over a
  much larger corpus.
- The custom analytics report has no documented category, series, row, or total-cell budget. QML
  copies and scans the complete matrix for filtering, table widths, delegates, zero rows, and CSV.
  Codex Security classified the operator-mediated single-process availability path as LOW/P3.

These are risks, not proven regressions. No representative EXPLAIN or benchmark for this exact path
was supplied, so no optimization is approved by this document.

### 12.3 Security and robustness

No direct SQL injection was found in the inspected query builders; values are bound and identifiers
come from catalogs. No SAM command-injection path was confirmed. Path traversal controls,
cross-process locks, savepoints, cancellation, and journaled consolidation are meaningful strengths.

Residual robustness risk is primarily local integrity/provenance and availability, not a confirmed
remote exploit. Codex Security scan `57dc551a-2b4e-4255-b399-e4c53d9edf84` reported two LOW/P3
findings with complete coverage of its 14-file runtime diff inventory:

- malformed primary SSA silent omission, fixed after the sealed snapshot and covered by mapper plus
  full-rescan rollback regressions;
- unbounded analytics report cardinality, still OPEN because a numeric limit requires measured
  production cardinality and a product decision.

The same scan rejected CSV injection/unsafe-write candidates and classified trend-aware zero hiding
as a functional mismatch rather than a vulnerability. The trend mismatch is fixed locally: only
observed values decide whether a category has no occurrences; trend values remain presentation data.

## 13. Test and scanner evidence

### 13.1 Tests executed during this audit

| Evidence | Result | Limit |
| --- | --- | --- |
| Baseline focused CTest selection | 177 selected; 176 passed; 1 symlink test skipped; 0 failed; 8.09 s | Pre-fix characterization, not full cross-platform suite |
| Baseline Qt/QML offscreen selection | 5 of 5 passed; 63.17 s | Pre-fix; screenshot text was not human-readable |
| Canonical Windows dev build after ISO slice | 160 of 160 build steps passed | Native Windows amd64 only |
| Focused ISO CTest selection | 6 of 6 passed | Proves 2020-W53 valid, 2021-W53 invalid, import-policy use, analytics validation, and advanced-filter bounds |
| Canonical Windows incremental build after integrity slice | 14 of 14 build steps passed | Native Windows amd64 only |
| Focused import-integrity CTest selection | 4 of 4 passed | Proves malformed-reference visibility, SAM rollback path, writer rejection, and preserved generic file isolation |
| Portable SAM impossible-week regression | Final run passed 1 of 1 | Proves a 2021-W53 artifact is rejected, no SSA table persists, database integrity holds, and the source artifact remains |
| Final incremental Windows test build | 4 of 4 build steps passed | Rebuilt the portable SAM regression on native Windows amd64 |
| Final canonical Windows incremental build | 22 of 22 build steps passed | Includes the invalid-pair query fix and positive SAM 2020-W53 coverage |
| Final focused regression selection | 12 of 12 passed; 0 failed; 2.78 s | Covers ISO validation, query fallback rejection, SAM rollback, positive week 53, malformed references, generic isolation, and QML smoke |
| all_qmllint | Passed | Static QML target only |
| Canonical Windows incremental build after wording slice | 8 of 8 build steps passed | Native Windows amd64 only |
| Focused analytics QML offscreen selection | 3 of 3 passed | Startup/chart/window behavior; not golden-pixel proof |
| Nonmutating qmlformat comparison | The two changed lines match formatter output | Full files retain unrelated baseline formatting differences; --check was unsupported and the timed-out command was interrupted, never counted as success |
| Existing import corpus record | 1,692 files and 96,479 final records with integrity check OK | Historical run, not repeated in this documentation slice |
| Custom-report configured QML lint | Passed | Covers the module after zero, table, CSV, and ISO-selector changes |
| Custom-report focused offscreen regressions | 6 of 6 passed | Dynamic ISO boundaries, exact-zero/null distinction, export actions, and PNG/SVG/CSV writes |
| Full activity-analytics window suite | 41 of 41 passed; 0 failed | Native Windows amd64 offscreen runtime |
| Custom-report visual artifact | 1580x940 PNG generated and inspected | Windows fonts plus Qt Basic style; not a golden baseline |
| Exact writer regression | Passed in ActivityAnalyticsViewModelTest | Proves atomic replacement and byte-exact CRLF output |
| Final clean Windows dev build | 575 of 575 build steps passed | Native Windows 11 amd64, MSVC, Qt 6.11.1; identical source later committed in `ffdb594` and `3ff8ab1` |
| Malformed primary SSA focused regressions | 2 of 2 passed | Mapper counts the row invalid and full rescan preserves the previous database |
| Final full Windows CTest | 657 of 657 passed; 0 failed; 5 symlink contracts skipped; 125.26 s | Native Windows amd64; skips are explicit platform-dependent contracts |
| Final zero/trend QML window suite | 41 of 41 passed; 0 failed; 49.90 s | Observed zero hides even with nonzero trend; null stays visible; zero list remains complete |
| Final custom-report visual artifact | 1580x940 PNG regenerated and inspected | Chart, main table, four-zero table, and CSV actions are visible without clipping |
| Clean Windows release build | 575 of 575 build steps passed | Native Windows 11 amd64, MSVC, Qt 6.11.1; manifest commit `152f2da2e703` |
| Full Windows release CTest | 657 of 657 passed; 0 failed; 5 symlink contracts skipped; 110.30 s | Same explicit platform-dependent skips as the dev gate |
| Windows release package | ZIP, portable, installer, and standalone generated | Version 0.9.17; portable `--version` passed; ZIP contains 1,660 entries; SHA256SUMS matches every top-level artifact |

The focused tests prove the narrow working-tree fixes. They do not prove the desired SAM formulas,
every SAM end-to-end branch, reverse-order database equivalence, other native platforms, or visual
glyph correctness.

### 13.2 Scanner evidence

| Tool | Result | Qualification |
| --- | --- | --- |
| Semgrep | Final changed-production scan: 62 rules, 6 files, zero findings | Complete for changed production files; an earlier broad scan timed out on large files and is not repo-wide proof |
| Gitleaks | Zero leaks across 2,550,886,647 bytes | Completed for the repository working tree |
| TruffleHog | Git history and all 24 deliverable files: zero verified or unknown secrets | Completed without timeout |
| detect-secrets | All 24 deliverable files passed; baseline blob stayed `1a733dfdeddb2724754ce038f5e56ec1606082fa` | Nonmutating invocation; `.secrets.baseline` was not edited |
| Codex Security final diff gate | Scan `a0e610eb-96d0-4741-9fcd-559089775dd3`: complete 14-file runtime-diff coverage; 1 LOW/P3 finding | Malformed-primary omission is closed; report cardinality remains OPEN pending a measured product threshold |
| CodeRabbit CLI | Unavailable | `coderabbit` was not installed on the native Windows host; no CodeRabbit finding count is claimed |
| C++ and QML static gate | diff-check, clang-format, qmlformat, all_qmllint, clang-tidy, and native cppcheck completed | Zero new diagnostic in changed lines; 12 P3 clang-tidy diagnostics are pre-existing and outside the patch; the WSL-bound cppcheck hook itself remains a harness issue |

Zero findings from an incomplete scanner must not be described as proof of absence.

### 13.3 External primary references used

- SQLite date/time and julianday:
  https://www.sqlite.org/lang_datefunc.html
- SQLite dynamic typing:
  https://www.sqlite.org/datatype3.html
- SQLite transactions and one simultaneous writer:
  https://www.sqlite.org/lang_transaction.html
- QSaveFile commit semantics:
  https://doc.qt.io/qt-6/qsavefile.html
- QSqlDatabase thread affinity:
  https://doc.qt.io/qt-6/qsqldatabase.html
- Qt threading overview:
  https://doc.qt.io/qt-6/threads.html
- Qt QDate ISO-week rules:
  https://doc.qt.io/qt-6/qdate.html
- Official ITAIPU procurement portal calendar page:
  https://compras.itaipu.gov.py/portal/ExibeConteudo.aspx?q=efg3MHqKBO4BuT78i0fPBQ%3D%3D
- ITAIPU 2026 work calendar in Portuguese, Annex I DET/AD-AE/0006/2025:
  https://compras.itaipu.gov.py/upload/Calendario2026_PT.pdf
- Paraguay Law 7544/2025:
  https://www.bacn.gov.py/leyes-paraguayas/12908/ley-n-75442025-que-determina-los-feriados-nacionales-de-la-republica-del-paraguay-se-establecen-los-feriados-moviles-y-se-faculta-al-poder-ejecutivo-a-instituir-otros-feriados-en-situaciones-especiales
- Brazil Portaria MGI 11.460/2025 PDF:
  https://legis.sigepe.gov.br/sigepe-bgp-ws-legis/legis-service/download/?id=0026440285-ALPDF%2F2025
- Qt 6.11.1 release:
  https://www.qt.io/blog/qt-6.11.1-released
- Qt SVG security advisory:
  https://www.qt.io/blog/security-advisory-type-confusion-and-heap-buffer-overflow-vulnerability-in-qt-svg-marker-handling

The local Qt version inspected was 6.11.1. The project target inventory did not show an SVG target
in the inspected CMake configuration. This is dependency context, not a finding in the application.

## 14. Ponytail annotations and concerns

This section is intentionally isolated. It records a complexity audit requested by the user and
does not authorize deletion or mix cleanup into SAM correctness work.

### 14.1 Suggestions returned by Ponytail

| Type | Suggestion | Estimated effect |
| --- | --- | --- |
| Delete | ThemePaletteLab, its CLI, tests, templates, and isolated CMake target | Remove an apparently isolated development tool |
| Delete | Optional Arrow snapshot inspector and dev-arrow preset | Remove optional Apache Arrow dependency surface |
| Shrink | Reuse domain::SsaExecutadasReportRow directly instead of a duplicate application adapter | Reduce duplicate result mapping |
| Delete | scripts/quick-build.sh | Remove an apparently unreferenced build path; no equivalent replacement was proven |
| Net estimate | Combined static estimate | About 1,200 lines and one optional dependency |

Paths named by the audit:

- tools/theme_lab/ThemePaletteLab.cpp
- tools/ssa_arrow_snapshot_inspector.cpp
- src/application/SsaExecutadasReportService.cpp
- scripts/quick-build.sh

### 14.2 Concerns before acting

1. Static caller search does not prove a developer tool has no release, operator, documentation, or
   manual consumer.
2. Removing a CMake target or preset changes supported workflows and requires dedicated approval
   plus host-specific validation.
3. The Arrow tool is optional and may be intentionally isolated; dependency reduction alone does
   not prove deletion value.
4. The Executadas adapter may be an intentional application boundary. Reusing a domain row could
   increase coupling even if it removes lines.
5. The quick script must be checked against external automation and contributor habits.
6. None of these suggestions fixes SAM report truth, import determinism, or data provenance.

### 14.3 Recorded disposition

- Status: ANNOTATION ONLY.
- Immediate action: none.
- Required before any future deletion: explicit user approval, reachability search, documentation
  search, CMake preset inventory, package/release impact, and all-host validation.
- These items must not enter a SAM implementation commit.

## 15. Ambiguity register

| ID | Question | Evidence state | Decision |
| --- | --- | --- | --- |
| A-01 | Which clock will the implementation use for urgent planning? | Source conflict C-01 remains documented | DECIDED 2026-08-09: 24 elapsed hours |
| A-02 | What exact field identifies operation-only R01 rows? | UNKNOWN C-04 | OPEN |
| A-03 | Which states define the R08 population? | UNKNOWN C-05 | OPEN |
| A-04 | Does desired R10 include SES as well as STE? | UNKNOWN | OPEN |
| A-05 | Are ALE/ASL or AIP/ASI authoritative for the SAM API? | CONFLICT C-06 | OPEN |
| A-06 | Can SCC appear in SAM artifacts? | UNKNOWN | OPEN |
| A-07 | Which versioned source supplies weekends and BR/PY holidays to the application? | G05 names Calendario do SOM; no application access contract is defined | Calendar source OPEN; fixed UTC-03:00 decided 2026-08-09 |
| A-08 | Which adjacent color owns each exact boundary? | Owner selected the least restrictive adjacent color | DECIDED 2026-08-09: 50 percent green; zero yellow |
| A-09 | Two files disagree about one SSA at the same update time. Which one wins, or should the import reject the conflict? | Current indicator behavior keeps whichever file is processed later | OPEN |
| A-10 | What traversal direction per relation category, depth, cycle rule, missing-node behavior, and result/paging limit define expanded R11? | UNKNOWN | OPEN |
| A-11 | How are not-applicable and unknown displayed and counted? | Current model conflates them | OPEN |
| A-12 | Which event source can prove deviation, partial, approval, and cancellation history? | Current snapshots insufficient | OPEN |
| A-13 | Is simple execution limited by G05's 48 hours or the report specification's programmed week, and from which start instant/clock? | CONFLICT and UNKNOWN C-08 | OPEN |
| A-14 | If business holidays are imported instead of read from SAM, which holiday set and revision are authoritative? | The public location calendar was Codex research; no SSA-to-location requirement was supplied | OPEN under D-02; no location mapping will be invented |
| A-15 | Should Historico de executadas disclose or prohibit fallback to programmed/registration week? | Current generic projection uses fallback; no SAM formula authorizes it | OPEN |

No default in this table is implied.

## 16. Reusable verification checklist

Before a future agent claims a SAM report is implemented:

1. Identify its exact population and exclusion rule.
2. Identify source fields and whether they are snapshot or event data.
3. Identify start, deadline, observation, clock kind, timezone, and calendar revision.
4. Prove not-applicable is distinct from unknown and zero.
5. Test before, at, and after 50 percent and deadline boundaries.
6. Test null, invalid, duplicate, late, canceled, and historical cases.
7. Trace SQL to port to application to ViewModel to QML.
8. Prove availability before the user can select the metric.
9. Validate labels against the actual denominator and formula.
10. Run a representative corpus benchmark and verify no full-table load into memory.
11. Run Windows, Debian, and macOS evidence on their native clones.
12. Record working tree, HEAD, local validation, and external service evidence separately.

## 17. Revision history

| Date | HEAD | Change |
| --- | --- | --- |
| 2026-08-06 | 2c93f51c9e6033da943f16738cce54c043ac896b | Initial verified knowledge base from PDFs, user specification, GLM audit, direct source inspection, tests, scanners, and independent agents |
| 2026-08-07 | 2c93f51c9e6033da943f16738cce54c043ac896b | Added primary calendar research, operational/corpus evidence, corrected GLM classifications, narrow uncommitted integrity/ISO/label fixes, and explicit working-tree validation |
| 2026-08-09 | 2c93f51c9e6033da943f16738cce54c043ac896b | Added custom-report zero/table/CSV/ISO-month behavior, malformed-primary validation, final Windows build/test evidence, and sealed Codex Security findings while preserving open business gates |
| 2026-08-09 | 152f2da2e70372b9b9a8e4ddf1a2ef9293c19493 | Committed the import/ISO, analytics, and separate documentation slices; generated and verified the full Windows release package; preserved every undecided business gate |
