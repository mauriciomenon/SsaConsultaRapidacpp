# SQLite Atomicity And Cancellation Contract

## Transaction Boundary

- Persistent DDL, delete, import writes, and derived-data mutations begin only
  after the transaction is open.
- Cancellation before commit rolls the transaction back explicitly. Rollback
  is not cancelable and its SQLite result is verified.
- Cancellation after commit does not rewrite the primary result as `Canceled`.
  The operation remains `Succeeded` and reports a warning for any interrupted
  optional consolidation or maintenance step.
- `VACUUM` and `ANALYZE` are post-commit maintenance. They are never part of
  the mutation transaction and cannot make a committed mutation look rolled
  back.

## Read And Busy Handling

- Each read operation owns a read-only SQLite connection. Query cancellation
  does not wait on a shared repository mutex.
- Progress and busy handlers observe the stop token with bounded polling. A
  locked database cannot turn cancellation into an unbounded busy timeout.
- A canceled or failed operation releases statements, connections, locks, and
  temporary files before its terminal is published.

## Error Boundary And Crash Safety

- Ports preserve SQLite result codes and technical detail for logging.
- GUI-facing results contain only a safe public message and a distinct
  diagnostic field that is never exposed to QML.
- Process-death tests kill the import writer before and after commit, reopen the
  database, run `PRAGMA integrity_check`, and accept only the complete previous
  state or the complete new state.
- A second read or write must succeed after rollback or interrupted execution.
