# Windows Build And Release Design

## Goal

Keep the Windows build reproducible and fast without polluting the repository
with mixed, stale or partially published artifacts.

## Directory contract

- `build/windows/<arch>/<toolchain>/<qt-kit>/<preset>` contains only
  rebuildable compiler products for one target.
- `dist/windows/<arch>/<toolchain>/releases/<version>-<sha>-windows-<arch>-<toolchain>`
  contains immutable final deliverables.
- `dist/windows/<arch>/<toolchain>/.staging/<run-id>` is owned by one package
  run and is removed on success or failure.
- `packaging/` contains maintained package definitions and documentation. It
  never contains generated artifacts.
- Legacy `build/dev`, `build/release` and `build/dev-wsl` are not canonical.

The first supported Windows layout is:

```text
build/windows/amd64/msvc/msvc2022_64/release
build/windows/amd64/llvm/msvc2022_64/release
build/windows/amd64/msvc/msvc2022_64/dev
build/windows/arm64/msvc/msvc2022_arm64/release
```

Arm64 must use its own Qt kit, compiler target and vcpkg triplet. An artifact
cannot be named arm64 until its PE machine type is validated.

## Release contract

Native command failures stop the package immediately. A run builds and checks
all deliverables inside its owned staging directory. Only a complete release
set is promoted. Failure preserves the previously promoted release.

The primary formats are:

- installer for normal users;
- ZIP extracted once for portable use.

The self-extracting EXE is legacy because it expands the full Qt runtime at
every launch. It is not part of the long-term release contract.

Tagged releases are immutable. A repeated version and SHA is accepted only
when hashes match. Local cleanup is explicit and never runs as a side effect of
packaging.

## Performance contract

Instrumentation must separate process start, native child start, analytics,
QML creation, first frame and first page. Optimization follows measured
milestones. Analytics may move after the first frame only with cancellation,
database-switch and shutdown coverage.

## Validation policy

Each slice runs only focused tests that protect its changed behavior. Broad
build, CTest and security gates run at the final release boundary, not after
every documentation or script edit.
