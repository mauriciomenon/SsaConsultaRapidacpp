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

## Future Port Families

- `IImportWorkflowPort`: external XLS/XLSX import, incremental rescan, full rescan, and input
  consolidation. Contract exists in `src/ports/IWorkflowPorts.h`.
- `IExportPort`: export the full filtered result set to supported output formats, starting at page
  1 of the filtered query. CSV export exists for CLI and GUI through `CsvExportPort`.
- `IDatabaseMaintenancePort`: reset, clean data, load other database, vacuum/analyze, and backup.
  SQLite reset, SQLite cleanup, and vacuum/analyze exist for CLI. Load-other-database and backup
  remain future adapters.
- `IDerivadasPort`: sync derivadas and provide graph/tree data.

## Layer Rules

- `presentation` can build a command request, but cannot execute OS, SQLite maintenance, import, or
  export logic directly.
- `platform` handles desktop integration such as opening URLs and paths.
- Future import, export, DB maintenance, and derivadas adapters must live outside `presentation` and
  be reached through their specialized ports.
- QML can only call view model methods. It cannot build command paths, SQL, import rules, or
  derivadas rules.
