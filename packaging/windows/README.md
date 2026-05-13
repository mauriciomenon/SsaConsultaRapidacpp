# Windows Packaging

Target: Windows 11 amd64 first, arm64 later after dependency validation.

## Build prerequisites

- Visual Studio 2022 Build Tools with "Desktop development with C++".
- Qt 6.11.0 for MSVC 2022 64-bit.
- CMake 3.24+.
- Ninja.

Expected local Qt path:

```powershell
C:\Qt\6.11.0\msvc2022_64
```

If Qt is upgraded, use either `QT_DIR` or `-QtDir`.

## Configure

From Developer PowerShell for VS 2022:

```powershell
.\tools\configure-dev.ps1
```

Explicit path:

```powershell
.\tools\configure-dev.ps1 -QtDir "C:\Qt\6.11.0\msvc2022_64"
```

## Build and test

```powershell
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Deploy local runnable folder

```powershell
$env:QT_DIR = "C:\Qt\6.11.0\msvc2022_64"
$env:Path = "$env:QT_DIR\bin;$env:Path"
& "$env:QT_DIR\bin\windeployqt.exe" .\build\dev\ssa_consulta_rapida.exe
```

Do not package real databases, local configs, logs, screenshots, or secrets.
