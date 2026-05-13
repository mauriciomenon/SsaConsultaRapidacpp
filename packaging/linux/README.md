# Linux Packaging

Target: Debian amd64 first, then Debian arm64 and Arch/Artix.

Target candidates: AppImage, tarball, or distro package.

## Debian build prerequisites

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build libsqlite3-dev \
  qt6-base-dev qt6-base-dev-tools \
  qt6-declarative-dev qt6-tools-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls
```

## Configure

```bash
./tools/configure-dev.sh
```

If Qt is installed outside system paths:

```bash
QT_DIR=/path/to/qt ./tools/configure-dev.sh
```

## Build and test

```bash
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Do not package real databases, local configs, logs, screenshots, or secrets.
