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
  include-what-you-use lcov sonar-scanner pre-commit gitleaks trufflehog
uv tool install cmakelang          # provides cmake-format and cmake-lint
pip3 install detect-secrets        # or: uv tool install detect-secrets
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
| cppcheck | C++ static analysis (fast) | `cppcheck --enable=all --inconclusive --suppress=missingIncludeSystem ...` |
| qmllint (Qt) | QML linting | `qmllint -I build/dev <qml files>` |
| qmlformat (Qt) | QML formatting | `qmlformat <qml files>` |
| gitleaks | Secret scanning | `gitleaks dir . --redact --exit-code 1` |
| trufflehog | Secret scanning | `trufflehog filesystem .` |
| detect-secrets | Secret scanning | `detect-secrets --baseline .secrets.baseline` |
| semgrep | SAST + security audit | `semgrep --config=p/c --config=p/security-audit` |

### Fixed (already present, corrected this cycle)

- `pre-commit` binary existed but no project hook config was wired. The CLI is
  now available at version 4.6.0; a `.pre-commit-config.yaml` is the next step
  (see "Next steps" below) to actually run linters on every commit.

### Newly installed this cycle

| Tool | Purpose | Install | Status |
|---|---|---|---|
| include-what-you-use 0.26 | Detect unused/missing C++ includes | `brew install include-what-you-use` | OK |
| lcov 2.4 + genhtml | Test coverage HTML reports | `brew install lcov` | OK |
| cmake-format 0.6.13 | CMake files formatting | `uv tool install cmakelang` | OK |
| cmake-lint 0.6.13 | CMake files linting | `uv tool install cmakelang` | OK |
| sonar-scanner 8.1.0 | SonarQube analysis uploader | `brew install sonar-scanner` | OK |

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

### Next steps (configuration, not installation)

The tools above are installed but not yet wired into the project config. Pending
explicit approval, the remaining work is configuration only:

1. `.pre-commit-config.yaml`: wire clang-format, clang-tidy, cppcheck, gitleaks,
   detect-secrets, cmake-format, qmllint, qmlformat to run on every commit.
2. IWYU: note it requires the Homebrew clang toolchain to match the build; since
   the build uses Apple clang, IWYU shows `<array> not found`. Running IWYU would
   require a separate build using Homebrew clang (blocked on the Apple-SDK issue).
3. SonarQube: a `sonar-project.properties` plus a running SonarQube server
   (via Apple Container, see below) to receive `sonar-scanner` uploads.

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
qmllint -I build/dev app/desktop/qml/Main.qml app/desktop/qml/Theme.qml app/desktop/qml/components/*.qml
find src app tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | \
  xargs -0 /opt/homebrew/opt/llvm/bin/clang-format --dry-run --Werror
SDKROOT="$(xcrun --show-sdk-path)"
PATH="/opt/homebrew/opt/llvm/bin:$PATH" run-clang-tidy -p build/dev -quiet \
  -header-filter="$PWD/(src|app)/.*" \
  -extra-arg=-isysroot -extra-arg="$SDKROOT" \
  "$PWD/(src|app)/.*\\.(cpp|h)$"
gitleaks dir . --redact --exit-code 1 --no-banner
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
