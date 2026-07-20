# SSA Consulta Rapida Cpp

Versao C++20 + Qt 6/QML da interface grafica do SSA Consulta Rapida.

Esta base nao e um port linha-a-linha da GUI Python. Ela preserva contratos de uso e aparencia geral, mas separa dominio, consulta, infraestrutura e apresentacao.

## Estado operacional

Consulte [`ROUND_STATUS.md`](ROUND_STATUS.md) antes de avaliar sincronizacao,
publicacao ou CI. Neste repositorio, `origin` aponta para GitLab, `bitbucket`
aponta para Bitbucket e `gh` aponta para o mirror GitHub atualmente indisponivel.

Para transferencia de contexto da v0.9.15, inclusive erros conhecidos e
pendencias canonicas, leia
[`docs/plans/2026-07-20-v0.9.15-glm-5.2-handoff.md`](docs/plans/2026-07-20-v0.9.15-glm-5.2-handoff.md).
O guia para reproduzir o relevo glossy de `ssa-dark` esta em
[`docs/development/theme-authoring.md`](docs/development/theme-authoring.md).

## Comandos rapidos

### Binarios, CLI e arquivos gerados

O projeto gera dois entrypoints diferentes:

- GUI: `build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida`
- CLI: `build/dev/ssa_consulta_rapida_cli`

O CLI atual nao e uma TUI interativa. Ele e um command-line entrypoint para consulta, detalhes, exportacao e manutencao:

```bash
./build/dev/ssa_consulta_rapida_cli --db /path/para/ssas.db --page-size 10
./build/dev/ssa_consulta_rapida_cli --db /path/para/ssas.db --details <numero_ssa>
./build/dev/ssa_consulta_rapida_cli --db /path/para/ssas.db --search MEG2 --export /tmp/ssas.csv
```

Arquivos de build ficam dentro do repo, em pasta ignorada:

```text
build/dev/
build/dev/ssa_consulta_rapida.app
build/dev/ssa_consulta_rapida_cli
build/dev/ssa_unit_tests
build/dev/ssa_integration_tests
build/dev/ssa_qt_presentation_tests
```

Os scripts de smoke usam `build/runtime/macos` para copia runtime de `ssas.db`, preferencias e screenshot. Os executaveis nao ficam no tmp.
O caminho default de banco para scripts sem argumentos e `data/ssas.db` dentro deste repo. Esse arquivo e ignorado pelo Git.

Para preparar o banco local do repo C++:

```bash
mkdir -p data
cp /path/para/ssas.db data/ssas.db
```

Artefatos de distribuicao, quando gerados por `./scripts/package-macos.sh`, ficam em `dist/macos/<arch>/`, tambem ignorado por git.

### Build e run por SO (padrao de scripts)

```bash
# macOS (padrao dev)
./scripts/build-macos.sh
./scripts/run-macos.sh

# Self executable path (macOS)
# <repo>/build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida

# Debian/Ubuntu (padrao dev)
./scripts/build-debian.sh
./scripts/run-debian.sh

# Self executable path (Debian)
# <repo>/build/dev/ssa_consulta_rapida
```

```powershell
# Windows 11 (executar no Developer PowerShell for VS 2022)
./scripts/build-windows.ps1
./scripts/run-windows.ps1

# Self executable path (Windows)
# <repo>\build\dev\ssa_consulta_rapida.exe
```

### Smoke completo (build + test + execucao com screenshot) no macOS e Debian

```bash
# Contrato canonico: clean, build, CTest, captura offscreen validada e exit.
./run-macos-smoke-clean

# Mesmo fluxo, seguido de janela visual aberta ate o operador fecha-la.
./run-macos-smoke-clean --open

# Help on the default flow
./scripts/smoke-macos.sh --help

# Parameterized smoke flow (stored in lazy scripts)
./scripts/lazy_scripts/macos-build-clean-test-smoke-run.sh
./scripts/lazy_scripts/macos-build-test-smoke-run.sh /path/para/ssas.db --help

# Parameterized build/run flows
./scripts/lazy_scripts/build-macos.sh --preset dev
./scripts/lazy_scripts/run-macos.sh /path/para/ssas.db --project-root "$REPO_ROOT" --screenshot /tmp/main.png
./scripts/lazy_scripts/run-debian.sh /path/para/ssas.db --project-root "$REPO_ROOT"
./scripts/lazy_scripts/build-windows.ps1 -Preset dev
.\scripts\lazy_scripts\run-windows.ps1 -DbPath C:\\caminho\\ssas.db -ProjectRoot C:\\path\\to\\SSA_Consulta_Rapida_Cpp -Screenshot C:\\tmp\\main.png

# Run from custom db path with parameterized lazy script
./scripts/lazy_scripts/run-macos.sh /path/para/ssas.db --project-root "$REPO_ROOT" --screenshot /tmp/main.png
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

## Clang-format

Verifique instalacao:

```bash
clang-format --version
```

macOS (recomendado):

```bash
brew install llvm
cat <<'EOF' >> ~/.zshrc
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
EOF
cat <<'EOF' >> ~/.bashrc
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
EOF
source ~/.zshrc
```

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y clang-format
```

Comando rapido para sessao atual (sem alterar perfil):

```bash
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
export PATH="/usr/bin:$PATH"
```

Windows 11:

```powershell
winget install --id LLVM.LLVM
```

Adicione `C:\Program Files\LLVM\bin` no `PATH` da sessao atual:

```powershell
$llvmBin = "C:\Program Files\LLVM\bin"
$env:Path = "$llvmBin;$env:Path"
```

Para injetar nos arquivos de perfil do PowerShell:

```powershell
if (-not (Test-Path $PROFILE)) { New-Item -Path $PROFILE -ItemType File -Force | Out-Null }
Add-Content -Path $PROFILE -Value '$env:Path = "C:\Program Files\LLVM\bin;$env:Path"'
```

Para recarregar o perfil e validar:

```powershell
. $PROFILE
clang-format --version
```

## Smoke visual

O wrapper canonico faz clean de `build/dev`, configura Qt 6.11, compila,
executa toda a suite e exige uma captura offscreen nova:

```bash
./run-macos-smoke-clean
```

Ele usa `data/ssas.db`, copia banco e preferencias para
`build/runtime/macos/`, remove qualquer screenshot antigo antes dos gates e
so retorna zero se build, CTest, app offscreen e gravacao do novo PNG passarem.
O screenshot fica em `build/runtime/macos/main.png`. Para abrir a GUI depois
do mesmo preflight use `./run-macos-smoke-clean --open`.

## Packaging e artefatos finais por plataforma

Comandos padrao sem argumentos (sem variavel de ambiente e sem parametros) geram pasta final persistente em `dist/<so>/<arquitetura>/`:

```bash
./scripts/package-macos.sh      # gera dist/macos/<arch>/ssa_consulta_rapida-<ver>-<arch>-macos.* (.zip e .dmg)
./scripts/package-linux.sh      # comando principal para Linux tarball
./scripts/package-linux.sh --skip-tests    # gera o mesmo pacote sem executar ctest
./scripts/package-debian.sh     # wrapper de compatibilidade; nao gera pacote .deb
```

```powershell
./scripts/package-windows.ps1   # gera dist/windows/<arch>/ssa_consulta_rapida-<ver>-<arch>-windows.zip e installer .exe opcional
./scripts/package-windows.ps1 -SkipTests   # gera o mesmo pacote sem executar ctest
```

Existem wrappers com argumento em `scripts/lazy_scripts` para cenarios com controle manual:

```bash
./scripts/lazy_scripts/package-macos.sh --preset <preset> --arch <arch> --dist-dir <dir>
./scripts/lazy_scripts/package-debian.sh --preset <preset> --arch <arch> --dist-dir <dir>
.\scripts\lazy_scripts\package-windows.ps1 -Preset <preset> -Arch <arch> -DistDir <dir>
```

Formatos persistentes criados:

- macOS: `ssa_consulta_rapida-<ver>-<arch>-macos.zip` e `ssa_consulta_rapida-<ver>-<arch>-macos.dmg`
  pacote persistente em `ssa_consulta_rapida-<ver>-<arch>-macos/ssa_consulta_rapida.app` e `run-ssa_consulta_rapida.sh`
- Debian/Linux: `ssa_consulta_rapida-<ver>-<arch>-linux.tar.gz`
  pacote persistente em `ssa_consulta_rapida-<ver>-<arch>-linux/` com `bin/`, `lib/` e `run-ssa_consulta_rapida.sh`
- Windows: `ssa_consulta_rapida-<ver>-<arch>-windows.zip` e `ssa_consulta_rapida-<ver>-<arch>-windows-installer.exe` (quando MakeNSIS esta instalado)
  pacote persistente em `ssa_consulta_rapida-<ver>-<arch>-windows/` com `ssa_consulta_rapida.exe` e `run-ssa_consulta_rapida.bat`

Todos os pacotes sao gerados apos build+test no preset `release` e sao organizados em `dist/<so>/<arch>/`.
Cada pacote cria tambem o ponteiro `dist/<so>/<arch>/latest` para o artefato atual.
No Windows, esses ponteiros sao links quando o sistema permite; caso contrario, os scripts usam copia como fallback.
Com base no ponteiro `latest`, ficam fixos tambem:

- `dist/linux/<arch>/latest-binary` (self sufficient wrapper script)
- `dist/linux/<arch>/latest-raw` (raw binary in `bin/`, requires `LD_LIBRARY_PATH` from `lib/`)
- `dist/linux/<arch>/latest-run.sh` (same launcher)
- `dist/linux/<arch>/latest.tar.gz`
- `dist/macos/<arch>/latest.app`
- `dist/macos/<arch>/latest-binary`
- `dist/macos/<arch>/latest-run.sh`
- `dist/macos/<arch>/latest.zip`
- `dist/macos/<arch>/latest.dmg`
- `dist/windows/<arch>/latest.exe` (installer alias, only when generated)
- `dist/windows/<arch>/latest-binary` (portable application executable)
- `dist/windows/<arch>/latest-run.bat`
- `dist/windows/<arch>/latest.zip`
- `dist/windows/<arch>/latest-installer.exe` (quando gerado)

Execucao direta dos artefatos persistentes:

```bash
./dist/linux/<arch>/ssa_consulta_rapida-<ver>-<arch>-linux/run-ssa_consulta_rapida.sh --project-root <repo> --db <db>
./dist/linux/<arch>/latest/bin/ssa_consulta_rapida --project-root <repo> --db <db>
./dist/linux/<arch>/latest-binary --project-root <repo> --db <db>
./dist/linux/<arch>/latest-raw --project-root <repo> --db <db> # requer LD_LIBRARY_PATH
./dist/linux/<arch>/latest-run.sh --project-root <repo> --db <db>
./dist/macos/<arch>/ssa_consulta_rapida-<ver>-<arch>-macos/run-ssa_consulta_rapida.sh --project-root <repo> --db <db>
./dist/macos/<arch>/latest/run-ssa_consulta_rapida.sh --project-root <repo> --db <db>
./dist/macos/<arch>/latest-binary --project-root <repo> --db <db>
./dist/macos/<arch>/latest.app/Contents/MacOS/ssa_consulta_rapida --project-root <repo> --db <db>
./dist/macos/<arch>/latest.dmg
./dist/macos/<arch>/latest.zip
```

```powershell
.\dist\windows\<arch>\ssa_consulta_rapida-<ver>-<arch>-windows\ssa_consulta_rapida.exe --project-root <repo> --db <db>
.\dist\windows\<arch>\latest\ssa_consulta_rapida.exe --project-root <repo> --db <db>
.\dist\windows\<arch>\latest-binary --project-root <repo> --db <db>
.\dist\windows\<arch>\latest-run.bat
.\dist\windows\<arch>\latest.zip
.\dist\windows\<arch>\latest.exe
.\dist\windows\<arch>\latest-installer.exe
```

## Execucao

macOS:

```bash
./build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida \
  --db "${REPO_ROOT}/data/ssas.db"
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
