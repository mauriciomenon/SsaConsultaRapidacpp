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
# Windows 11 (o script inicializa sozinho o Developer PowerShell MSVC x64)
./scripts/build-windows.ps1
./scripts/run-windows.ps1

# Self executable path (Windows)
# <repo>\build\windows\amd64\msvc2022_64\dev\ssa_consulta_rapida.exe
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
.\tools\configure-dev.ps1 -QtDir "D:\Qt\6.11.1\msvc2022_64"
.\tools\configure-dev.ps1 -SQLiteRoot "C:\vcpkg\installed\<triplet>"
```

### Verificacao de toolchain

- No Windows, `.\scripts\build-windows.ps1` localiza o modulo do Visual Studio
  e inicializa target x64 ou ARM64 conforme `-Arch`. Nao e necessario abrir
  Developer PowerShell manualmente.
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
.\tools\configure-dev.ps1 -QtDir "D:\Qt\6.11.1\msvc2022_64"
```

Se o SQLite estiver em outro prefixo:

```powershell
.\tools\configure-dev.ps1 `
  -QtDir "C:\Qt\6.11.1\msvc2022_64" `
  -SQLiteRoot "C:\vcpkg\installed\<triplet>"
```

## Requisitos

- CMake 3.24+
- Ninja
- Qt 6.6+ com Core, Gui, Qml, Quick, QuickControls2, Sql e Test
- SQLite3
- Compilador C++20

O CI e a referencia de deteccao em `tools/qt-detect.conf` usam Qt 6.11.0. Os
scripts aceitam qualquer patch da familia 6.11.x e selecionam o mais alto
valido. Versoes validadas no desenvolvimento local atual:

- Windows 11 amd64: Qt 6.11.1 `mingw_64` e MinGW GCC 13.1.
- Debian/WSL amd64: Qt 6.11.1 `gcc_64` e GCC 14.2.
- macOS arm64: Qt 6.11.x e Apple Clang, validados em rodadas anteriores.

Ordem dos toolchains Windows para este projeto:

| Ordem | Toolchain | Estado |
| --- | --- | --- |
| 1 | MinGW GCC 13.1 com Qt `mingw_64` | Default; build e 610 testes validados |
| 2 | MSVC 19.51 com Qt `msvc2022_64` | Build GUI validado com SDK 10.0.26100.0 |
| 3 | LLVM-MinGW 17.0.6 com Qt `llvm-mingw_64` | Incompativel com `std::stop_token` |
| 4 | clang-cl 22.1.3 com Qt MSVC | Nao suportado nesta versao; use somente para diagnostico |

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

Os modos `--check`/`--check-package` e `-Check`/`-CheckPackage` imprimem
`OK`, `MISSING` ou `UNSUPPORTED` e encerram antes da configuracao CMake.

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
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config libsqlite3-dev sqlite3 \
  libdbus-1-dev libgl1-mesa-dev libegl1 libopengl0 \
  libxkbcommon-dev libxkbcommon-x11-0 libxcb-cursor0 \
  binutils dpkg-dev fakeroot file tar zip
```

Debian Trixie fornece Qt 6.8.x, que nao atende a familia 6.11.x exigida pelo
preflight. Instale Qt 6.11.x `linux_gcc_64` pelo
[Qt Online Installer](https://doc.qt.io/qt-6/get-and-install-qt.html), apenas
com Qt Base e Qt Declarative. WebEngine nao e necessario. O caminho convencional
e `~/Qt/<versao>/gcc_64`.

Depois:

```bash
./tools/configure-dev.sh
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

O comando `pacman` e a politica Qt 6.11.x de Arch/Artix estao em
[packaging/linux/README.md](packaging/linux/README.md).

### Windows 11

- Instale [Visual Studio 2026 ou Build Tools](https://visualstudio.microsoft.com/downloads/)
  com C++ desktop, Qt 6.11.x `msvc2022_64`, CMake, Ninja e vcpkg standalone.
- Instale SQLite com `vcpkg install sqlite3:x64-windows` ou o triplet do alvo.
- Instale [NSIS](https://nsis.sourceforge.io/Download); `makensis.exe` e
  obrigatorio para o EXE portatil unico e para o instalador.
- Consulte [packaging/windows/README.md](packaging/windows/README.md) para os
  downloads oficiais e o bootstrap completo.

Seu caminho esperado hoje:

```powershell
C:\Qt\6.11.1\msvc2022_64
```

`sqlite3.h` e `sqlite3.lib` sao dependencias de build; a `sqlite3.dll`
correspondente entra no pacote. O usuario final nao instala SQLite nem altera
PATH.

Build direto:

```powershell
$env:QT_DIR = "C:\Qt\6.11.1\msvc2022_64"
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
.\tools\configure-dev.ps1 -QtDir "D:\Qt\6.11.1\msvc2022_64"
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
$env:QT_DIR = "C:\Qt\6.11.1\msvc2022_64"
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

O procedimento completo esta em [Build, Run, Test](docs/development/build-run-test.md).

Os scripts oficiais usam somente o preset `release` e preservam `dist/` durante
`./scripts/make_clean`. Para uma entrega completa, nao use `--skip-tests` ou
`-SkipTests`; esses switches existem apenas para iteracao de empacotamento.

```bash
./scripts/package-macos.sh
./scripts/package-debian.sh
./scripts/package-linux.sh
./scripts/package-arch.sh
```

```powershell
.\scripts\package-windows.ps1
```

### Pasta final canonica

O nome base e sempre o nome do repositorio: `ssa_consulta_rapida_cpp`.
Uma compilacao bem-sucedida atualiza as copias sem versao:

| Plataforma | Diretorio | Arquivos |
| --- | --- | --- |
| Windows | `dist/windows/<arch>/final/` | wrapper `.exe`, instalador, ZIP e pasta `ssa_consulta_rapida_cpp-standalone/` |
| Debian | `dist/linux/<arch>/final/` | wrapper, `.deb`, ZIP e pasta `ssa_consulta_rapida_cpp-standalone/` |
| macOS | `dist/macos/<arch>/final/` | `ssa_consulta_rapida_cpp.app`, `ssa_consulta_rapida_cpp.dmg`, `ssa_consulta_rapida_cpp.zip` |

Os arquivos sem sufixo `-standalone` sao os wrappers portateis historicos: eles
extraem o runtime para uma area temporaria antes de iniciar. O instalador e
separado. Para executar sem wrapper, use o binario dentro da pasta
`*-standalone`; ele e o PE/ELF real e mantem Qt, plugins, QML e SQLite ao lado
do executavel. Nao ha instalacao separada de Qt ou SQLite.

Os caminhos diretos sao:

```text
Windows: dist/windows/<arch>/final/ssa_consulta_rapida_cpp-standalone/ssa_consulta_rapida_cpp.exe
Debian:  dist/linux/<arch>/final/ssa_consulta_rapida_cpp-standalone/ssa_consulta_rapida_cpp
```

Quando `HEAD` esta limpo e aponta exatamente para `v<version>`, os mesmos
scripts criam uma copia imutavel com a versao no final do nome:

```text
ssa_consulta_rapida_cpp-<version>.exe
ssa_consulta_rapida_cpp-installer-<version>.exe
ssa_consulta_rapida_cpp-<version>.deb
ssa_consulta_rapida_cpp-<version>.app
ssa_consulta_rapida_cpp-<version>.dmg
ssa_consulta_rapida_cpp-<version>.zip
```

Builds sujos ou commits sem a tag exata atualizam somente os nomes sem versao.
Uma versao final existente nunca e sobrescrita. Os aliases legados `latest*`
continuam disponiveis fora de `final/` para compatibilidade.

### Execucao direta

```bash
./dist/linux/<arch>/final/ssa_consulta_rapida_cpp --db <db>
open ./dist/macos/<arch>/final/ssa_consulta_rapida_cpp.app
```

```powershell
.\dist\windows\<arch>\final\ssa_consulta_rapida_cpp.exe --db <db>
.\dist\windows\<arch>\final\ssa_consulta_rapida_cpp-installer.exe
.\dist\windows\<arch>\final\ssa_consulta_rapida_cpp-standalone\ssa_consulta_rapida_cpp.exe --db <db>
```

No Debian, execute o ELF diretamente:

```bash
./dist/linux/<arch>/final/ssa_consulta_rapida_cpp-standalone/ssa_consulta_rapida_cpp --db <db>
```

Na primeira abertura sem `--project-root`, a GUI cria sua raiz local em
`~/.ssaconsultarapida` (Windows: `%USERPROFILE%\.ssaconsultarapida`). A falta
de `data/ssas.db` e valida: a aplicacao abre para permitir a primeira carga. As
preferencias e os logs ficam em `config/` abaixo da mesma raiz.
`--project-root`, `--db` e `--config-dir` permitem escolher os caminhos
explicitamente.

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
$env:QT_DIR = "C:\Qt\6.11.1\msvc2022_64"
$env:Path = "$env:QT_DIR\bin;$env:Path"
.\build\dev\ssa_consulta_rapida.exe `
  --db C:\caminho\para\ssas.db `
  --config-dir "$env:LOCALAPPDATA\SSA_Consulta_Rapida_Cpp"
```

Para gerar uma pasta executavel fora do ambiente de build no Windows:

```powershell
& "$env:QT_DIR\bin\windeployqt.exe" .\build\dev\ssa_consulta_rapida.exe
```

`tools\configure-dev.ps1` usa o prefixo Qt apenas na configuracao e nao altera
`QT_DIR` nem o `Path` global. Para executar ferramentas Qt manualmente, defina
`QT_DIR` na sessao ou chame o caminho completo:

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
