# External Commands Contract

## Purpose

External commands are desktop integration operations outside the query and presentation model. They
are exposed through `IExternalCommandPort` so QML and view models never call OS APIs directly.
Import, export, SQLite maintenance, and derivadas synchronization require their own ports.

## Result Model

Every command returns one explicit status:

- `Succeeded`: the command was accepted and completed or was started asynchronously.
- `NotImplemented`: the command is part of the functional roadmap but has no C++ adapter yet.
- `Rejected`: the command request is invalid, incomplete, or unsafe.
- `Failed`: the command was valid but execution failed.

No command may silently ignore unsupported behavior. `NotImplemented` is valid during migration, but
the UI and CLI must be able to show it.

## Command Kinds

- `OpenSamHome`: open the SAM home URL.
- `OpenSsa`: open one SSA in SAM; requires `ssa_number`.
- `OpenPath`: open a local file or directory; requires `path` and an allowed root.
- `OpenInputFolder`: open the configured input files folder.
- `OpenProcessedFolder`: open the configured processed-files folder.
- `OpenRedundantFolder`: open the configured redundant-files folder.
- `OpenInstallationGuide`: open the configured installation documentation.

## Specialized Port Families

- `IImportWorkflowPort`: external XLSX import, incremental rescan, full rescan,
  and input consolidation are implemented. SSA discovery never invokes the
  isolated legacy XLS converter. See `ssa-import.md`.
- `IExportPort`: export the full filtered result set to supported output formats, starting at page
  1 of the filtered query. CSV export exists for CLI and GUI through `CsvExportPort`.
- `IDatabaseMaintenancePort`: reset, clean data, vacuum/analyze, and backup.
  SQLite reset, SQLite cleanup, and vacuum/analyze exist for CLI. Backup remains
  a future adapter.
- `IDatabaseValidator` and `IApplicationLauncher`: validate another database in
  read-only mode and start a replacement application instance with `--db`.
  They are separate from database maintenance because the current connection
  is never switched in place.
- `IDerivadasPort`: orphan-reference cleanup exists. Graph/tree reads use the
  repository/query path. Explicit derivadas source import remains a separate
  pending workflow.
- `ISamRefreshPort`: fetches a bounded all-sector REST batch through the local
  `scrap_report` project and discards temporary artifacts. Runtime import stays
  disabled by default until the SAM schema adapter and truncation proof pass
  end to end. Each sector is currently limited to 200 records.

## Layer Rules

- `presentation` can build a command request, but cannot execute OS, SQLite maintenance, import, or
  export logic directly.
- `platform` handles desktop integration such as opening URLs and paths.
- Import, export, DB maintenance, and derivadas adapters must live outside `presentation` and
  be reached through their specialized ports.
- SAM process execution belongs to `platform`; batch acceptance and import
  orchestration belong to `application`; timer and visible settings belong to
  `presentation`.
- QML can only call view model methods. It cannot build command paths, SQL, import rules, or
  derivadas rules.
- Help and About are local presentation surfaces and must not execute network,
  subprocess, database, or Git operations.

## Supervised Process Contract

- Every external process is started through `SupervisedProcess`; direct
  `QProcess` lifecycle ownership is not allowed in presentation or QML.
- Unix and macOS create a dedicated process session and terminate the complete
  process group. Windows assigns the process at creation to a Job Object with
  `KILL_ON_JOB_CLOSE`.
- A start intent is registered before `QProcess::start`; the concrete tree is
  registered on `QProcess::started`, before a fast leader can escape tracking.
  A force request prevents new starts, reports `Pending` until every start and
  tree drains, and never blocks the GUI thread.
- Normal cancellation uses short polling, graceful termination, forced kill,
  and confirmation before publishing `Canceled`.
- Force shutdown reports `Drained` only with zero starts pending and zero active
  trees. Failed termination remains fail-closed and exits with failure after
  the bounded desktop barrier; cleanup failure cannot be reported as safe.
- Staged files are copied in blocks to a temporary destination. Publication is
  one rename after complete success; cancel or failure removes the temporary.
