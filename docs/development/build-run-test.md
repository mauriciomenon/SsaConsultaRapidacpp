# Build, Run, Test

## macOS

O fluxo completo usado pelo operador e o wrapper versionado na raiz:

```bash
./run-macos-smoke-clean
```

Esse comando resolve o symlink para `scripts/run-macos-smoke-clean.sh`, remove
somente `build/dev`, reconfigura o preset `dev` com Qt 6.11, compila, executa
`ctest --preset dev --output-on-failure`, copia `data/ssas.db` para o runtime
isolado e gera um screenshot offscreen novo. Captura antiga e removida antes
dos gates; template ausente, build, CTest, launch, watchdog ou arquivo vazio
retornam falha.

Para abrir a GUI somente depois do mesmo preflight validado:

```bash
./run-macos-smoke-clean --open
```

A janela interativa permanece aberta ate o operador encerra-la. Ela nao faz
parte do contrato deterministico sem argumentos.

Para o mesmo clean, build e teste em automacao, sem janela persistente, use o
smoke offscreen. Ele gera `build/runtime/macos/main.png`:

```bash
./scripts/smoke-macos.sh
```

Para iteracao incremental, sem remover `build/dev`:

```bash
export SSA_CPP_PRESET=dev
./scripts/build-macos.sh
ctest --preset dev --output-on-failure
```

Para reconstruir release, testar e gerar ZIP/DMG:

```bash
./scripts/package-macos.sh
```

Para verificar a versao compilada depois do build:

```bash
./build/dev/ssa_consulta_rapida_cli --version
```

Nao crie diretorios de build alternativos para contornar falhas. Os unicos
presets oficiais sao `dev` e `release`; scripts de clean, smoke e package usam
esses mesmos diretorios e preservam `data/`, `dist/` e configuracoes locais.

## Entregas finais locais

Os scripts de pacote mantem a ultima entrega bem-sucedida em:

```text
dist/windows/<arch>/final/ssa_consulta_rapida_cpp.exe
dist/windows/<arch>/final/ssa_consulta_rapida_cpp-installer.exe
dist/windows/<arch>/final/ssa_consulta_rapida_cpp.zip
dist/windows/<arch>/final/ssa_consulta_rapida_cpp-standalone/ssa_consulta_rapida_cpp.exe
dist/linux/<arch>/final/ssa_consulta_rapida_cpp
dist/linux/<arch>/final/ssa_consulta_rapida_cpp.deb
dist/linux/<arch>/final/ssa_consulta_rapida_cpp.zip
dist/linux/<arch>/final/ssa_consulta_rapida_cpp-standalone/ssa_consulta_rapida_cpp
dist/macos/<arch>/final/ssa_consulta_rapida_cpp{.app,.dmg,.zip}
```

Os arquivos Windows e Debian sem `-standalone` sao wrappers autoextraiveis para
compatibilidade. O executavel de teste e entrega direta fica em
`*-standalone/ssa_consulta_rapida_cpp[.exe]`, com o runtime Qt, plugins, QML e
SQLite na mesma pasta. Ele pode ser chamado diretamente, sem `cmd`, shell
wrapper, PATH de Qt ou extracao temporaria.

Na primeira abertura normal, a aplicacao cria
`~/.ssaconsultarapida` (Windows: `%USERPROFILE%\.ssaconsultarapida`). A falta
de `data/ssas.db` e valida: a GUI abre para permitir a primeira carga.
Preferencias e logs ficam em `config/` abaixo da mesma raiz. Os smoke tests e
os scripts de pacote passam `--project-root` e `--config-dir` para usar um
runtime controlado.

Quando o working tree esta limpo e `HEAD` aponta exatamente para
`v<version>`, uma copia com `-<version>` no final do nome e criada sem
sobrescrever releases anteriores. Builds intermediarios atualizam somente os
nomes sem versao. `./scripts/make_clean` nunca remove essas entregas.

O warning `cmake_minimum_required` sob `.deps-cache/miniz-src` vem do source
cache da dependencia. Nao edite o cache gerado. A correcao pertence a uma
atualizacao validada do pin em `cmake/Dependencies.cmake`.

Os scripts aceitam apenas a familia Qt 6.11.x. `QT_DIR`, `Qt6_DIR` e
`CMAKE_PREFIX_PATH` sao escolhas explicitas e vencem a autodeteccao. Sem essas
variaveis, o configurador procura os patches 6.11.x instalados e usa o mais
alto valido; se ele estiver incompleto, continua em ordem decrescente. Para
limitar a busca a uma raiz nao padrao, use `QT_INSTALL_ROOT=/caminho/Qt`.

Consulte a escolha sem configurar nem compilar:

```bash
env -u QT_DIR -u Qt6_DIR -u CMAKE_PREFIX_PATH \
  ./tools/configure-dev.sh --print-qt-prefix
```

Em macOS e Linux, o compilador pode ser fixado com `CC` e `CXX`. Quando mais
de um compilador e encontrado, o configurador mostra a sintaxe de selecao. O
banco default dos smokes e `data/ssas.db`; sua ausencia encerra o script com
erro objetivo.

Execucao manual do binario, quando necessaria:

```bash
./build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida \
  --db /caminho/para/ssas.db
```

## Linux

Use o guia canonico de [Linux](../../packaging/linux/README.md) para os comandos
`apt-get` de Debian/Ubuntu, `pacman` de Arch/Artix e a instalacao separada do Qt
6.11.x. Debian Trixie fornece Qt 6.8.x, que nao atende a familia configurada.
O configurador procura Qt em `QT_DIR`, `Qt6_DIR`, `CMAKE_PREFIX_PATH`,
`QT_INSTALL_ROOT`, `~/Qt/*/gcc_64` e caminhos de sistema.
O CI e a referencia de deteccao em `tools/qt-detect.conf` usam Qt 6.11.0; o
host local desta rodada usou Qt 6.11.1. O configurador aceita patches da
familia 6.11.x.

## Windows

Use o guia canonico de [Windows](../../packaging/windows/README.md) para links
oficiais, vcpkg/SQLite e requisitos de package. A busca padrao usa `C:\Qt`,
seleciona o patch 6.11.x mais alto valido e usa MSVC em amd64 e arm64.
Use `-Toolchain` para selecionar clang-cl/LLVM ou MinGW explicitamente.

```powershell
# Preflight somente leitura.
.\tools\configure-dev.ps1 -Check
.\tools\configure-dev.ps1 -CheckPackage

# Default amd64: MSVC.
.\scripts\build-windows.ps1

# Build ARM64 em namespace e kit proprios.
.\scripts\build-windows.ps1 -Arch arm64

# Selecao explicita por compilador.
.\build-windows.ps1 -Toolchain mingw
.\build-windows.ps1 -Toolchain llvm-mingw
.\build-windows.ps1 -Toolchain msvc
.\build-windows.ps1 -Toolchain llvm

# Raiz de instalacao alternativa.
.\scripts\build-windows.ps1 -Toolchain msvc -QtRoot "D:\Qt"

# Somente inspecao, sem configurar ou compilar.
.\tools\configure-dev.ps1 -QtRoot "C:\Qt" -PrintQtSelection

# Run, smoke e pacote aceitam o mesmo seletor.
.\run-windows.ps1 -Toolchain mingw
.\run-windows-smoke-clean.ps1 -Toolchain mingw
.\package-windows.ps1 -Toolchain mingw

# CTest Windows usa o diretorio namespaced.
ctest --test-dir .\build\windows\amd64\mingw\mingw_64\dev --output-on-failure
```

Os builds Windows ficam somente em
`build/windows/<arch>/<toolchain>/<qt-kit>/<preset>`. Assim, compiladores
distintos nao dividem cache entre si nem com `build/dev` ou `build/release`
de Linux e macOS.

Os kits `wasm_singlethread` e `wasm_multithread` geram WebAssembly para
navegadores. Eles sao listados no diagnostico, mas nunca sao fallback para o
aplicativo desktop; um alvo WASM exigiria preset e empacotamento proprios.

O `windeployqt` e o `macdeployqt` continuam sendo resolvidos a partir do cache
do build. Assim, a ferramenta de deploy usa exatamente o mesmo patch Qt do
binario e nao mistura patches diferentes da familia 6.11.x.

## Matriz de compiladores

| Plataforma | Compilador | Estado atual |
| --- | --- | --- |
| Windows amd64 | MinGW GCC 13.1 | Build real, 610 testes e smoke sem DB validados |
| Windows amd64 | MSVC 19.51 | Build release, 610 testes, pacote e startup sem DB validados |
| Windows amd64 | LLVM-MinGW 17.0.6 | Falha: biblioteca C++ nao fornece `std::stop_token` |
| Windows amd64 | clang-cl 22.1.3 + lld-link | Build release, 610 testes, pacote e startup sem DB validados |
| Debian/WSL amd64 | GCC 14.2 | Build e 641 testes validados |
| Debian/WSL amd64 | Clang 19.1.7 | Suportado pelo fonte, sem gate completo nesta rodada |
| macOS arm64 | Apple Clang | Historicamente validado; nao executado nesta rodada |

`msvc2022_64`, `llvm-mingw_64` e `mingw_64` sao produtos Qt com ABIs
diferentes. Mistura entre eles e `UNSUPPORTED`. O fato de clang-cl usar ABI
MSVC nao basta para declarar o kit Qt MSVC validado; esse par exige um gate
completo proprio. Kits `wasm_*` tambem nao sao alvos deste aplicativo desktop.

### Medicao Windows Debug

Medicao de 2026-07-24, build limpo apenas do alvo GUI e segunda abertura
offscreen sem banco:

| Toolchain | Build GUI | EXE Debug | Primeiro frame | Analytics |
| --- | ---: | ---: | ---: | ---: |
| MinGW GCC 13.1 | 198,029 s | 195.277.539 bytes | 165,211 ms | 10,682 ms |
| MSVC 19.51 | 141,620 s | 13.201.920 bytes | 2.237,971 ms | 13,510 ms |
| LLVM-MinGW 17.0.6 | falhou em 56,757 s | - | - | - |

MSVC venceu o tempo de compilacao, mas MinGW venceu claramente o startup Debug
nesta maquina. O initializer de analytics nao explica a lentidao: o intervalo
dominante do MSVC ocorreu depois dele e antes da primeira consulta/QML. Nao
extrapolar esses numeros para pacote Release sem medir o binario Release.

Testes isolados da deteccao:

```bash
bash tests/scripts/qt-detection-tests.sh
pwsh -NoLogo -NoProfile -Command \
  "Invoke-Pester -Path 'tests/scripts/QtDetection.Tests.ps1' -CI"
```

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
| --- | --- | --- |
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
| --- | --- | --- | --- |
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
| --- | --- | --- | --- |
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

## Linux build container legado (Debian Trixie)

O `scripts/container/Containerfile.debian-build` instala Qt 6.8.x do Debian
Trixie. Esse container nao satisfaz atualmente o preflight Qt 6.11.x e nao deve
ser apresentado como build canonico ate receber o mesmo kit Qt do CI/host.

### Contents

- **Compilers**: g++ 14, clang 14, mingw-w64 (cross to Windows)
- **Build**: cmake 3.31, ninja, make, automake, autoconf, libtool, pkg-config
- **Linters**: clang-tidy, clang-format, cppcheck 2.10
- **Qt 6.8** (inventario legado; `UNSUPPORTED` pelo preflight Qt 6.11.x atual)
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
  --db "$HOME/path/to/ssa-consulta-rapida/data/ssas.db" \
  --screenshot /tmp/ssa-cpp-smoke.png \
  --smoke-exit-ms 1500
```
