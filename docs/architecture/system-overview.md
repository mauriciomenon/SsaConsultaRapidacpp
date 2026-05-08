# System Overview

## Current Truth

This repository is a new C++20 + Qt 6/QML implementation of the SSA Consulta Rapida GUI.
It preserves user-facing behavior from the Python GUI, but it does not port the Python
architecture.

## Boundaries

- `domain`: records, columns, paging, filter expressions, and value types. No Qt.
- `query`: search parser, SQL query compiler, and query service. No Qt.
- `ports`: interfaces used by use cases. No Qt.
- `infra`: SQLite and preferences persistence.
- `platform`: desktop/OS integration.
- `presentation`: Qt view models and table models.
- `app/desktop/qml`: visual composition only.

## Main Flow

1. QML calls `MainViewModel`.
2. `MainViewModel` builds `SsaPageRequest`.
3. `SsaQueryService` calls `ISsaRepository`.
4. `SqliteSsaRepository` uses `SqlQueryBuilder` and returns domain records.
5. `SsaTableModel` exposes the current page to QML.

## Non Goals In First Cycle

- Import/rescan implementation.
- Derivadas synchronization implementation.
- Production installers beyond macOS app bundle zip.
- Business aliases or semantic search.
