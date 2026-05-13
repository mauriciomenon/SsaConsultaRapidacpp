# macOS Packaging

First artifact target: app bundle zip.

## Build prerequisites

```bash
brew install qt cmake ninja sqlite
```

Expected Qt prefix on Apple Silicon:

```bash
/opt/homebrew/opt/qt
```

Expected Qt prefix on Intel:

```bash
/usr/local/opt/qt
```

## Configure

```bash
./tools/configure-dev.sh
```

Explicit path:

```bash
QT_DIR=/opt/homebrew/opt/qt ./tools/configure-dev.sh
```

## Build and test

```bash
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Required before publishing:

- Configure release preset.
- Build release preset.
- Launch app bundle with a writable runtime.
- Run QML smoke against a fixture database.
- Create zip from the fresh `.app` bundle.
