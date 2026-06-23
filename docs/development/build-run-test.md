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
