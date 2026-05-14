# SSA Consulta Rapida Cpp

Versao C++20 + Qt 6/QML da interface grafica do SSA Consulta Rapida.

Esta base nao e um port linha-a-linha da GUI Python. Ela preserva contratos de uso e aparencia geral, mas separa dominio, consulta, infraestrutura e apresentacao.

## Comandos rapidos

### Build e run por SO (padrao de scripts)

```bash
# macOS
./scripts/build-macos.sh
./scripts/run-macos.sh /path/para/ssas.db --screenshot /tmp/main.png

# Debian/Ubuntu
./scripts/build-debian.sh
./scripts/run-debian.sh /path/para/ssas.db --config-dir "$HOME/.config/ssa-consulta-rapida-cpp"
```

```powershell
# Windows 11 (executar no Developer PowerShell for VS 2022)
./scripts/build-windows.ps1
./scripts/run-windows.ps1 -DbPath C:\caminho\ssas.db -Screenshot C:\tmp\main.png
```

### Ordem de deteccao usada nos scripts

#### `tools/configure-dev.sh`
- 1o: `QT_DIR`
- 2o: `CMAKE_PREFIX_PATH`
- 3o: instalacao padrao por SO (`/opt/homebrew/opt/qt`, `/usr/local/opt/qt`, `/usr/lib/cmake/Qt6/*`)
- 4o: instalacao local (`$HOME/Qt`, etc)

#### `tools/configure-dev.ps1`
- 1o: parametro `-QtDir`
- 2o: `QT_DIR` ou `CMAKE_PREFIX_PATH` (via variaveis de ambiente)
- 3o: `C:\Qt\<QT_VERSION>\msvc2022_64` lido de `tools/qt-detect.conf`
- 4o: `C:\Qt\*\msvc2022_64`

### Caminhos customizados quando mudar de local

```bash
# macOS / Debian
QT_DIR=/meu/qt ./tools/configure-dev.sh
```

```powershell
# Windows
.\tools\configure-dev.ps1 -QtDir "D:\Qt\6.11.0\msvc2022_64"
.\tools\configure-dev.ps1 -SQLiteRoot "C:\vcpkg\installed\<triplet>"
```

### Verificacao de toolchain

- No Windows, rode `.\scripts\build-windows.ps1` dentro de um shell com `cl.exe` no PATH (`vswhere` + `vcvarsall`).
- Em caso de erro de SQLite, valide `vcpkg install sqlite3:<triplet>` e rode:

```powershell
.\tools\configure-dev.ps1 -SQLiteRoot "C:\vcpkg\installed\<triplet>"
```

O script tenta detectar Qt nesta ordem:

- `-QtDir` informado na linha de comando.
- `QT_DIR` ou `CMAKE_PREFIX_PATH`.
- `C:\Qt\<QT_VERSION>\msvc2022_64`, usando `QT_VERSION` de `tools\qt-detect.conf`.
- `C:\Qt\*\msvc2022_64`, usando o subdiretorio definido em `tools\qt-detect.conf`.

O script tenta detectar SQLite nesta ordem:

- `-SQLiteRoot` informado na linha de comando.
- `SQLite3_ROOT` ou `SQLITE_ROOT`.
- `VCPKG_ROOT\installed\<triplet>`.
- `C:\vcpkg\installed\<triplet>`.

O `<triplet>` vem de `VCPKG_DEFAULT_TRIPLET`; se nao estiver definido, o script
usa `arm64-windows` em sessao ARM64 e `x64-windows` nos demais casos.

Se a versao ou o caminho mudar:

```powershell
.\tools\configure-dev.ps1 -QtDir "D:\Qt\6.11.0\msvc2022_64"
```

Se o SQLite estiver em outro prefixo:

```powershell
.\tools\configure-dev.ps1 `
  -QtDir "C:\Qt\6.11.0\msvc2022_64" `
  -SQLiteRoot "C:\vcpkg\installed\<triplet>"
```

## Requisitos

- CMake 3.24+
- Ninja
- Qt 6.6+ com Core, Gui, Qml, Quick, QuickControls2, Sql e Test
- SQLite3
- Compilador C++20

Versao local usada no desenvolvimento atual:

- Qt 6.11.0
- macOS arm64 com Homebrew
- Windows 11 com Visual Studio 2022 Build Tools
- Debian/Ubuntu com pacotes Qt 6 do sistema

## Configuracao rapida

Use os scripts abaixo para detectar Qt e configurar o preset `dev`.

macOS ou Debian:

```bash
./tools/configure-dev.sh
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Windows 11 PowerShell:

```powershell
.\tools\configure-dev.ps1
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Os scripts tentam detectar:

- `QT_DIR`
- `CMAKE_PREFIX_PATH`
- Qt instalado em caminhos comuns
- `cmake`
- `ninja`
- compilador C++ disponivel

Se a deteccao falhar, o script imprime o caminho esperado e uma sugestao de instalacao.

A versao alvo e caminhos padrao ficam centralizados em:

```text
tools/qt-detect.conf
```

## Instalacao de dependencias

### macOS

Com Homebrew:

```bash
brew install qt cmake ninja sqlite
```

Se o Homebrew estiver em caminho padrao, o script usa:

```bash
/opt/homebrew/opt/qt
```

Em Macs Intel, o caminho comum e:

```bash
/usr/local/opt/qt
```

Tambem e possivel informar explicitamente:

```bash
QT_DIR=/opt/homebrew/opt/qt ./tools/configure-dev.sh
```

### Debian ou Ubuntu

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build libsqlite3-dev \
  qt6-base-dev qt6-base-dev-tools \
  qt6-declarative-dev qt6-tools-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls
```

Depois:

```bash
./tools/configure-dev.sh
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

### Windows 11

- Instale Visual Studio 2022 Build Tools com "Desktop development with C++".
- Instale Qt 6.11.0 para MSVC 2022 64-bit.
- Instale CMake e Ninja.
- Instale SQLite para MSVC, preferencialmente com `vcpkg install sqlite3:<triplet>`.
- Rode os comandos no "Developer PowerShell for VS 2022".

Seu caminho esperado hoje:

```powershell
C:\Qt\6.11.0\msvc2022_64
```

Build direto:

```powershell
$env:QT_DIR = "C:\Qt\6.11.0\msvc2022_64"
$env:SQLite3_ROOT = "C:\vcpkg\installed\<triplet>"
$env:Path = "$env:QT_DIR\bin;$env:Path"
cmake --preset dev `
  -DCMAKE_PREFIX_PATH="$env:QT_DIR" `
  -DSQLite3_INCLUDE_DIR="$env:SQLite3_ROOT\include" `
  -DSQLite3_LIBRARY="$env:SQLite3_ROOT\lib\sqlite3.lib"
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Build com deteccao:

```powershell
.\tools\configure-dev.ps1
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Se o SQLite nao for detectado:

```powershell
.\tools\configure-dev.ps1 -SQLiteRoot "C:\vcpkg\installed\<triplet>"
```

Se o Qt mudar de versao, o script procura automaticamente por:

```powershell
C:\Qt\*\msvc2022_64
```

Tambem e possivel passar o caminho:

```powershell
.\tools\configure-dev.ps1 -QtDir "D:\Qt\6.11.0\msvc2022_64"
```

## Build manual

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
$env:QT_DIR = "C:\Qt\6.11.0\msvc2022_64"
$env:SQLite3_ROOT = "C:\vcpkg\installed\<triplet>"
$env:Path = "$env:QT_DIR\bin;$env:Path"
cmake --preset dev `
  -DCMAKE_PREFIX_PATH="$env:QT_DIR" `
  -DSQLite3_INCLUDE_DIR="$env:SQLite3_ROOT\include" `
  -DSQLite3_LIBRARY="$env:SQLite3_ROOT\lib\sqlite3.lib"
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Validacao local

Depois de configurar e compilar, rode:

```bash
cmake --build --preset dev
ctest --preset dev --output-on-failure
qmllint -I build/dev app/desktop/qml/Main.qml app/desktop/qml/Theme.qml app/desktop/qml/components/*.qml
```

O `-I build/dev` e necessario porque o modulo QML `SsaConsultaRapida`
e o `qmldir` sao gerados pelo CMake dentro do diretorio de build.
Sem esse import path, o qmllint reporta falsos warnings de import e singleton.

No Windows PowerShell:

```powershell
cmake --build --preset dev
ctest --preset dev --output-on-failure
& "$env:QT_DIR\bin\qmllint.exe" -I build/dev app/desktop/qml/Main.qml app/desktop/qml/Theme.qml app/desktop/qml/components/*.qml
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
$env:QT_DIR = "C:\Qt\6.11.0\msvc2022_64"
$env:Path = "$env:QT_DIR\bin;$env:Path"
.\build\dev\ssa_consulta_rapida.exe `
  --db C:\caminho\para\ssas.db `
  --config-dir "$env:LOCALAPPDATA\SSA_Consulta_Rapida_Cpp"
```

Para gerar uma pasta executavel fora do ambiente de build no Windows:

```powershell
& "$env:QT_DIR\bin\windeployqt.exe" .\build\dev\ssa_consulta_rapida.exe
```

Se voce usou `tools\configure-dev.ps1`, `QT_DIR` fica definido na sessao atual,
mas o script nao altera `Path` globalmente. Para executar ferramentas Qt
manualmente, chame o caminho completo:

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
