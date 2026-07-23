# Windows Build And Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development where repository rules permit it.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce clean Windows builds and atomically published releases while
reducing startup overhead.

**Architecture:** Build outputs are namespaced below the existing `build/`
root. Packaging uses one run-owned staging directory and promotes one immutable
release set only after focused validation. Startup work begins with measured
milestones.

**Tech Stack:** PowerShell 7, Pester 5.7.1, CMake presets, CTest, Qt 6.11,
NSIS and GitHub Actions.

## Global Constraints

- Work directly on `master`; do not create a branch, worktree or PR.
- Preserve all unrelated dirty working-tree changes.
- Keep `packaging/` source-only and `dist/` final-only.
- Use focused tests per slice and broad validation only at the final boundary.
- Create atomic commits and push only the files owned by each slice.
- Do not publish to GitHub while the account returns HTTP 403.

---

### Task 1: Fail-fast Windows packaging

**Files:**
- Modify: `tests/scripts/WindowsBuildCache.Tests.ps1`
- Create: `tests/scripts/PackageWindows.Tests.ps1`
- Modify: `scripts/build-windows.ps1`
- Modify: `scripts/package-windows.ps1`
- Modify: `.github/workflows/release.yml`

**Interfaces:**
- Consumes: existing PowerShell script parameters.
- Produces: package commands that stop on native failure and use the Qt from
  the selected CMake cache.

- [ ] Replace the five legacy Pester assertions with Pester 5 syntax.
- [ ] Add one failing test for a failed build and one for cache-derived Qt.
- [ ] Run only the two affected Pester files and confirm the intended failures.
- [ ] Check `$LASTEXITCODE` after build, CTest and tar without adding a generic
  wrapper.
- [ ] Resolve `<prefix>/lib/cmake/Qt6` by ascending three levels.
- [ ] Align the workflow artifact name with the script output.
- [ ] Run Pester, PSScriptAnalyzer and a read-only diff review.
- [ ] Commit the slice.

### Task 2: Canonical build namespace

**Files:**
- Modify: `CMakePresets.json`
- Modify: `tools/configure-dev.ps1`
- Modify: `tools/configure-dev.sh`
- Modify: `scripts/build-windows.ps1`
- Modify: Windows run/package scripts that consume the build directory.
- Modify: focused script tests and build documentation.

**Interfaces:**
- Consumes: preset, platform, architecture and selected Qt toolchain.
- Produces: one canonical path below
  `build/<platform>/<arch>/<toolchain>/<preset>`.

- [ ] Back up `CMakePresets.json` with a timestamp.
- [ ] Add a failing path-ownership test.
- [ ] Make configure, build, test and package consume the same resolved path.
- [ ] Prove Windows and Linux paths cannot share a cache.
- [ ] Run focused PowerShell and shell tests.
- [ ] Commit the slice.

### Task 3: Transactional release set

**Files:**
- Modify: `scripts/package-windows.ps1`
- Modify: `scripts/lib/package_common.ps1`
- Modify: `tests/scripts/PackageWindows.Tests.ps1`
- Modify: Windows package documentation.

**Interfaces:**
- Consumes: complete staged installer and ZIP.
- Produces: immutable release directory plus one current manifest.

- [ ] Add a failing test proving a package failure preserves the current
  release.
- [ ] Stage under `.staging/<run-id>` and clean it in `finally`.
- [ ] Generate SHA-256 hashes and a release manifest.
- [ ] Promote the complete directory, then replace `current.json`.
- [ ] Keep cleanup as a separate explicit operation.
- [ ] Run focused Pester tests and a package dry-run fixture.
- [ ] Commit the slice.

### Task 4: Startup measurement and delivery format

**Files:**
- Modify: desktop startup/runtime files only where milestones are emitted.
- Create: one Windows benchmark runner under `tools/`.
- Modify: Windows delivery documentation.

**Interfaces:**
- Consumes: wrapper and native executable paths.
- Produces: JSON samples for first frame and first page.

- [ ] Add a failing parser test for required benchmark milestones.
- [ ] Record wrapper, process, analytics, QML, frame and query milestones.
- [ ] Compare native and self-extracting formats with controlled samples.
- [ ] Make installer and ZIP the documented primary formats when measurements
  confirm the wrapper overhead.
- [ ] Run focused tests and the Windows benchmark.
- [ ] Commit the slice.

### Task 5: Final validation and publication

- [ ] Run the canonical build and CTest suites for affected platforms.
- [ ] Run focused PowerShell analysis and package smokes.
- [ ] Run the repository security gates once for the complete change set.
- [ ] Verify release hashes, PE architecture and clean staging.
- [ ] Push `master` to `origin` and `bitbucket`, then verify both remote refs.
- [ ] Record external CI separately from local validation.
