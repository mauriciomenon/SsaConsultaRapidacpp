# Build, Run, Test

## macOS

```bash
brew install qt cmake ninja sqlite
cmake --preset dev -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Run:

```bash
./build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida \
  --db /Users/menon/git/SSA_Consulta_Rapida/data/ssas.db
```

## Linux

Install Qt 6 development packages, CMake, Ninja, SQLite, then use the same presets.

## Windows

Use Qt 6, CMake, Ninja, and a C++20 compiler. Windows packaging is planned after the
macOS bundle smoke is stable.

## Quality Toolchain

The project ships a layered toolchain. Tools are grouped by status so it is clear
what was already present, what was fixed, and what is newly available.

### Prerequisites (install once, macOS)

```bash
brew install qt cmake ninja sqlite llvm clang-format cppcheck \
  include-what-you-use lcov pre-commit gitleaks trufflehog
uv tool install cmakelang          # provides cmake-format and cmake-lint
uv tool install detect-secrets
```

Note: `include-what-you-use` exposes the binary `include-what-you-use` (the
community alias `iwyu` is not on PATH; use `iwyu_tool.py` for the wrapper).

### Already present and working (baseline)

These tools were already installed and functional before this cycle. They form
the core of the quality gate.

| Tool | Purpose | Command |
|---|---|---|
| clang-format (LLVM) | C++ formatting | `clang-format --dry-run --Werror <files>` |
| clang-tidy (LLVM) | C++ static analysis | `run-clang-tidy -p build/dev ...` |
| cppcheck | C++ static analysis (fast) | `pre-commit run cppcheck --hook-stage manual --files <changed production files>` |
| qmllint (Qt) | QML linting | `cmake --build --preset dev --target all_qmllint` |
| qmlformat (Qt) | QML formatting | `bash -o pipefail -c 'qmlformat "$1" \| diff -u "$1" -' -- <file>` |
| gitleaks | Secret scanning | `gitleaks dir . --redact --exit-code 1` |
| trufflehog | Secret scanning | `trufflehog git file://. ...`; use `trufflehog filesystem <changed-file> ...` for changed files |
| detect-secrets | Secret scanning | `detect-secrets-hook --baseline .secrets.baseline <changed-files>` |
| semgrep | SAST + security audit | `semgrep --config=p/c --config=p/security-audit` |

### Pre-commit conditions

The repository already provides `.pre-commit-config.yaml`. Install the hook once
with `pre-commit install`; all hooks use local system binaries and therefore
require the corresponding tools on `PATH`.

Default commit hooks follow these conditions:

- clang-format, cmake-format, and qmlformat rewrite staged file types. Pre-commit
  detects the diff and blocks until the formatted files are reviewed and staged.
- shellcheck blocks on findings in changed shell scripts.
- qmllint runs only when QML is staged, blocks on findings, and requires an
  existing `build/dev/build.ninja` configuration.
- the local Semgrep QML policy runs only when QML is staged, blocks on findings,
  and requires Semgrep 1.169.0 or newer.
- gitleaks, detect-secrets, and the changed-file TruffleHog hook block on secret
  findings. TruffleHog may call verifier APIs and treats scan errors as failures.

Run each manual hook with its intended scope:

```bash
production_files=()
while IFS= read -r -d '' file; do
  production_files+=("$file")
done < <(git diff --name-only -z --diff-filter=ACMRT HEAD -- src app tools)
if ((${#production_files[@]})); then
  pre-commit run cppcheck --hook-stage manual --files "${production_files[@]}"
fi
pre-commit run cmake-lint --hook-stage manual --all-files
pre-commit run gitleaks-full --hook-stage manual
pre-commit run trufflehog-history --hook-stage manual
```

Cppcheck receives changed production files only; tests and generated build
trees are excluded. Cmake-lint validates repository configuration files, while
gitleaks and TruffleHog scan Git history independently.

`detect-secrets scan --baseline .secrets.baseline` is a maintenance command: it
updates baseline metadata and must not be used as a read-only verification gate.

### Newly installed this cycle

| Tool | Purpose | Install | Status |
|---|---|---|---|
| include-what-you-use 0.26 | Detect unused/missing C++ includes | `brew install include-what-you-use` | OK |
| lcov 2.4 + genhtml | Test coverage HTML reports | `brew install lcov` | OK |
| cmake-format 0.6.13 | CMake files formatting | `uv tool install cmakelang` | OK |
| cmake-lint 0.6.13 | CMake files linting | `uv tool install cmakelang` | OK |

### Compiler-provided (no install needed; Apple clang 21)

These are compiler flags on the Apple clang 21 toolchain (`/usr/bin/clang`), not
packages. They are activated via dedicated CMake presets in `CMakePresets.json`.

Note: Homebrew LLVM 22 is also installed but does NOT compile against the Apple
SDK (known LLVM-upstream vs Apple-SDK mismatch). All sanitizer/coverage presets
use Apple clang 21, which fully supports them, plus C++23 (verified: `__cpp_lib_byteswap`,
`__cpp_lib_mdspan`, `__cpp_lib_format`, `__cpp_lib_ranges` all present).

| Preset | Flags | Purpose | Status |
|---|---|---|---|
| `dev-asan` | `-fsanitize=address,undefined` | ASan + UBSan runtime checks | validated: 100/100 tests pass |
| `dev-tsan` | `-fsanitize=thread` | TSan data-race detection | validated: builds; found 1 real race (see RECOVERY_BACKLOG) |
| `dev-cov` | `-fprofile-instr-generate -fcoverage-mapping` | Coverage instrumentation | validated: 85.35% lines, report at `build/dev-cov/coverage_html/` |

Rules of thumb (from LLVM docs): ASan + UBSan compose cleanly; TSan is mutually
exclusive with ASan, hence the separate build directory. Never ship
sanitizer/coverage binaries.

### Coverage report

```bash
./scripts/generate-coverage.sh      # builds report under build/dev-cov/coverage_html/
open build/dev-cov/coverage_html/index.html
```

The script runs `ctest --preset dev-cov`, merges profraw via `llvm-profdata`,
exports to lcov via `llvm-cov`, and renders HTML via `genhtml`. Requires the
`dev-cov` preset (configure + build once first).

### Known toolchain limitation

IWYU requires the Homebrew clang toolchain to match the build. Since the build
uses Apple clang, IWYU shows `<array> not found`. Running IWYU would require a
separate build using Homebrew clang, which is blocked on the Apple-SDK issue.

## Linux build container (Debian Trixie)

A `Containerfile.debian-build` at `scripts/container/` produces a Debian Trixie
image with all build tools and linters for Linux CI / reproducible builds.

### Contents

- **Compilers**: g++ 14, clang 14, mingw-w64 (cross to Windows)
- **Build**: cmake 3.31, ninja, make, automake, autoconf, libtool, pkg-config
- **Linters**: clang-tidy, clang-format, cppcheck 2.10
- **Qt 6.8** (Trixie ships Qt 6.8.x; project requires >= 6.6, Bookworm only has 6.4)
- **Tools**: gdb, bison, flex, gawk, lsd, git, sqlite3, curl, network utilities

### Build and run with Apple Container

```bash
container system start                       # one time, start the service
container system kernel set --recommended    # one time, install kata kernel
container build -f scripts/container/Containerfile.debian-build -t ssa-debian-build scripts/container
container run --rm -it -v "$PWD:/work" -w /work ssa-debian-build
```

The Containerfile is also Docker/Podman compatible (`docker build ...`).

### Known limitation: emulation performance on Apple Container

Apple Container 1.0.0 runs Linux containers via a lightweight VM. On Apple
silicon, running an `aarch64` Debian image is **native**, but a full Qt project
build is still slow (observed: ~37 objects compiled in 10+ minutes for the full
build). The environment is correct (configure succeeds, individual files
compile, all tools work), but a from-scratch full build is impractical for
interactive use due to VM overhead.

Practical uses that work well:
- Verifying Linux portability of individual files (`g++ -std=c++20 -c ...`)
- Running linters (cppcheck, clang-tidy) in the Linux environment
- CI on real Linux runners (where the Containerfile is most valuable)

For fast local Linux builds, prefer a native Linux machine or CI rather than
the emulated Apple Container. The OrbStack runtime (already installed) is an
alternative if faster emulation is needed.

## Local Gates

```bash
cmake --preset dev -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --build --preset dev --target all_qmllint
status=0
while IFS= read -r file; do
  bash -o pipefail -c 'qmlformat "$1" | diff -u "$1" -' -- "$file" || status=1
done < <(git ls-files '*.qml')
test "$status" -eq 0
find src app tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | \
  xargs -0 /opt/homebrew/opt/llvm/bin/clang-format --dry-run --Werror
SDKROOT="$(xcrun --show-sdk-path)"
PATH="/opt/homebrew/opt/llvm/bin:$PATH" run-clang-tidy -p build/dev -quiet \
  -header-filter="$PWD/(src|app)/.*" \
  -extra-arg=-isysroot -extra-arg="$SDKROOT" \
  "$PWD/(src|app)/.*\\.(cpp|h)$"
gitleaks dir . --redact --exit-code 1 --no-banner
changed_files=()
while IFS= read -r -d '' file; do
  changed_files+=("$file")
done < <(git diff --name-only -z --diff-filter=ACMRT HEAD)
if ((${#changed_files[@]})); then
  detect-secrets-hook --baseline .secrets.baseline "${changed_files[@]}"
  for file in "${changed_files[@]}"; do
    trufflehog filesystem "$file" --results=verified,unknown --fail \
      --fail-on-scan-errors --no-update
  done
fi
trufflehog git file://. --results=verified,unknown --fail \
  --fail-on-scan-errors --no-update
```

## macOS Package Smoke

```bash
mkdir -p dist
ditto -c -k --sequesterRsrc --keepParent \
  build/dev/ssa_consulta_rapida.app \
  dist/ssa_consulta_rapida-macos.zip
QT_QPA_PLATFORM=offscreen \
  build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida \
  --db /Users/menon/git/SSA_Consulta_Rapida/data/ssas.db \
  --screenshot /tmp/ssa-cpp-smoke.png \
  --smoke-exit-ms 1500
```
