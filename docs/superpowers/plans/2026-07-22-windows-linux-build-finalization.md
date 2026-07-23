# Windows And Linux Build Finalization Implementation Plan

> Historical correction (2026-07-22): the SQLite system PATH work described
> below was abandoned. The final implementation stages the matching
> `sqlite3.dll` beside each Windows executable and includes it in the portable
> package. End users do not install SQLite and no SQLite runtime directory is
> added to the system or user PATH.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the Windows and Linux build recovery, remove the remaining runtime and test failures, and publish atomic commits to GitLab and Bitbucket.

**Architecture:** Keep platform environment setup in the existing build scripts, stage application runtimes beside the executables, keep QML geometry data-driven through implicit sizing, and keep concurrent prefetch completion order outside the functional contract. Python remains an installer and fixture-tool dependency only; the application runtime stays C++20 and Qt 6.

**Tech Stack:** C++20, Qt 6.11.x (CI/reference 6.11.0; local validation 6.11.1),
QML, CMake, Ninja, PowerShell, Pester, Bash, CTest, uv.

## Global Constraints

- Preserve macOS arm64, Windows 11 amd64/arm64, Debian amd64/arm64, and Arch amd64/arm64 behavior.
- Do not add Node or Bun global dependencies.
- Use MSVC x64 and Qt `msvc2022_64` as the default Windows build.
- Keep the project version at `0.9.15`; no release bump was requested.
- Do not create a branch, worktree, PR, or merge.

---

### Task 1: Superseded Windows SQLite PATH Proposal

This proposal was rejected after review. A private application DLL does not
belong in Machine or User PATH. The accepted implementation resolves
`sqlite3.dll` from the SQLite prefix selected for linking, stages it beside the
GUI and CLI executables, and requires the portable package to contain it. The
PATH was left without a SQLite runtime entry.

### Task 2: Preserve The QML Import Button Natural Width

**Files:**
- Modify: `app/desktop/qml/Main.qml`
- Test: `cmake/RunHermeticGuiSmoke.cmake`

**Interfaces:**
- Consumes: `ActionButton.implicitWidth` and `RowLayout` sizing.
- Produces: `availableWidth >= contentWidth` for `mainImportXlsxButton` at 1580x940, 1180x940, and 1180x760.

- [ ] **Step 1: Verify RED**

Run the three Linux GUI smokes and confirm `availableWidth=92`, `contentWidth=93.8125`, and `fit=false`.

- [ ] **Step 2: Apply the minimal QML contract**

Add the following property only to `mainImportXlsxButton`:

```qml
Layout.minimumWidth: implicitWidth
```

- [ ] **Step 3: Review before build**

Run non-mutating `qmlformat`, `qmllint`, Semgrep, detect-secrets, Gitleaks, and TruffleHog checks for the modified file.

- [ ] **Step 4: Verify GREEN**

Rebuild and rerun all three GUI smokes on Linux and Windows. Confirm all 12 control-fit probes pass.

### Task 3: Remove The Invalid Prefetch Completion-Order Contract

**Files:**
- Modify: `tests/smoke/PresentationSmokeTest.cpp`
- Verify: `src/presentation/PageQueryCoordinator.cpp`

**Interfaces:**
- Consumes: three recorded repository requests for current page and two prefetch pages.
- Produces: proof that pages 0, 1, and 2 were requested exactly once without constraining worker completion order.

- [ ] **Step 1: Use the observed RED**

Retain the full-suite evidence where the completion vector was `0,2,1` and the old assertion required `0,1,2`.

- [ ] **Step 2: Change only the test contract**

Collect the three `pageIndex` values, sort the local copy, and compare it with `std::vector<std::size_t>{0, 1, 2}`. Do not change production scheduling.

- [ ] **Step 3: Review before build**

Run clang-format, clang-tidy on altered lines, Cppcheck, Semgrep, detect-secrets, Gitleaks, and TruffleHog.

- [ ] **Step 4: Verify GREEN and stability**

Run the focused test repeatedly, the complete presentation test binary, and the full Linux CTest suite.

### Task 4: Validate Python Tooling Boundaries

**Files:**
- Verify: `scripts/smoke-import-large-xlsx.sh`
- Update outside Git: `%USERPROFILE%\Downloads\SSA_BUILD_LESSONS_LEARNED.md`

**Interfaces:**
- Consumes: Python only for `aqtinstall` and shell-embedded fixture scripts.
- Produces: documented compatibility evidence for Python 3.13, 3.14, and 3.15 without adding a Python application dependency.

- [ ] **Step 1: Verify multiprocessing on native Linux temporary storage**

Run `multiprocessing.Manager` under Python 3.13.13, 3.14.5, and 3.15.0b2 with `TMPDIR=/tmp`; require all three to pass.

- [ ] **Step 2: Verify the installed Qt base kit**

Confirm `$HOME/Qt/6.11.1/gcc_64` exposes Core, Gui, Qml, Quick, QuickControls2, Sql, Concurrent, and Test. Do not reinstall or add WebEngine when the exact kit is already valid.

- [ ] **Step 3: Validate Python fixture syntax**

Extract or execute the embedded Python fixture blocks under all three interpreters with Linux-native temporary storage.

- [ ] **Step 4: Document the boundary**

Record that the product runtime is C++/Qt, while Python is limited to installer and test tooling.

### Task 5: Final Review, Atomic Commits, And Publication

**Files:**
- Stage only the recovery files verified in this cycle.
- Leave unrelated preexisting dirty files unstaged.

**Interfaces:**
- Consumes: green Windows/Linux builds, tests, static analysis, and secret scans.
- Produces: verified refs on `origin/master` and `bitbucket/master`.

- [ ] **Step 1: Run complete verification**

Run the canonical Windows and Linux builds, full CTest suites, QML lint, C++ static analysis, PowerShell/Shell checks, and secret scanners.

- [ ] **Step 2: Audit the staged diff**

Use `git diff --cached --check`, inspect every staged path, and verify that no local environment file, backup, secret, IDE directory, or unrelated dirty file is staged.

- [ ] **Step 3: Create atomic commits**

Commit the accumulated cross-platform build recovery separately from the final QML/test-contract fixes when the patch boundaries are separable without partial breakage.

- [ ] **Step 4: Push and verify both required remotes**

Push `master` and any new tags to `origin` and `bitbucket`, then fetch both and confirm the remote refs equal local `HEAD`. Do not push to `gh` while the account remains suspended.
