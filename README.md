# SSA Consulta Rapida Cpp

Versao C++20 + Qt 6/QML da interface grafica do SSA Consulta Rapida.

Esta base nao e um port linha-a-linha da GUI Python. Ela preserva contratos de uso e aparencia geral, mas separa dominio, consulta, infraestrutura e apresentacao.

## Requisitos

- CMake 3.24+
- Ninja
- Qt 6.6+ com Core, Gui, Qml, Quick, QuickControls2, Sql e Test
- SQLite3
- Compilador C++20

No macOS com Homebrew:

```bash
brew install qt cmake ninja sqlite
```

No Debian:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build libsqlite3-dev \
  qt6-base-dev qt6-base-dev-tools \
  qt6-declarative-dev qt6-tools-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls
```

No Windows 11:

- Instale Visual Studio 2022 Build Tools com "Desktop development with C++".
- Instale Qt 6 para MSVC 2022 64-bit.
- Instale CMake e Ninja.
- Rode os comandos no "Developer PowerShell for VS 2022".

## Build

macOS:

```bash
cmake --preset dev -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Debian:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Windows 11 PowerShell:

```powershell
$env:QT_DIR = "C:\Qt\6.8.3\msvc2022_64"
$env:Path = "$env:QT_DIR\bin;$env:Path"
cmake --preset dev -DCMAKE_PREFIX_PATH="$env:QT_DIR"
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Execucao

macOS:

```bash
./build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida \
  --db /Users/menon/git/SSA_Consulta_Rapida/data/ssas.db
```

Debian:

```bash
./build/dev/ssa_consulta_rapida \
  --db /caminho/para/ssas.db \
  --config-dir "$HOME/.config/ssa-consulta-rapida-cpp"
```

Windows 11 PowerShell:

```powershell
$env:QT_DIR = "C:\Qt\6.8.3\msvc2022_64"
$env:Path = "$env:QT_DIR\bin;$env:Path"
.\build\dev\ssa_consulta_rapida.exe `
  --db C:\caminho\para\ssas.db `
  --config-dir "$env:LOCALAPPDATA\SSA_Consulta_Rapida_Cpp"
```

Para gerar uma pasta executavel fora do ambiente de build no Windows:

```powershell
& "$env:QT_DIR\bin\windeployqt.exe" .\build\dev\ssa_consulta_rapida.exe
```

Se `--db` nao for informado, a aplicacao procura `data/ssas.db` abaixo de
`--project-root` ou do diretorio atual.

## Principios

- GUI/QML so visual.
- ViewModels Qt so coordenam estado visual e comandos.
- Query e dominio nao dependem de Qt.
- SQLite e paginado, com filtros compilados para SQL.
- Preferencias sao contrato versionado.
- Nenhum fallback silencioso.
