# Build And Packaging - Linux

Alvos: Debian/Ubuntu amd64 e arm64, seguidos por Arch/Artix. O projeto exige Qt
6.11.x. Debian Trixie fornece Qt 6.8.x e, portanto, seus pacotes Qt nao atendem
sozinhos ao contrato atual.

## Debian e Ubuntu

Instale compilador, build tools, SQLite, bibliotecas de plataforma e ferramentas
de pacote:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config \
  libsqlite3-dev sqlite3 \
  libdbus-1-dev \
  libgl1-mesa-dev libegl1 libopengl0 \
  libxkbcommon-dev libxkbcommon-x11-0 \
  libxcb-cursor0 \
  binutils dpkg-dev fakeroot file tar zip
```

Instale separadamente Qt 6.11.x `linux_gcc_64` pelo Qt Online Installer,
selecionando Qt Base e Qt Declarative:
https://doc.qt.io/qt-6/get-and-install-qt.html

O caminho convencional e `~/Qt/<versao>/gcc_64`. WebEngine e modulos extras
nao sao necessarios.

Somente em uma distribuicao que realmente ofereca Qt 6.11.x, os equivalentes
de sistema sao:

```bash
sudo apt-get install -y \
  qt6-base-dev qt6-base-dev-tools \
  qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick \
  qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts \
  qml6-module-qtquick-window \
  qml6-module-qtquick-templates
```

Confira a versao antes de usar esse segundo bloco. Qt 6.8.x e `UNSUPPORTED`
pelo preflight atual.

## Arch e Artix

```bash
sudo pacman -S --needed \
  base-devel cmake ninja sqlite \
  qt6-base qt6-declarative \
  libglvnd libxkbcommon dbus \
  tar gzip file zstd
```

Se os repositorios nao fornecerem Qt 6.11.x, instale o kit oficial
`linux_gcc_64` pelo Qt Online Installer e informe `QT_DIR` ou
`QT_INSTALL_ROOT`. Nao misture bibliotecas Qt do sistema com o kit oficial.

## Preflight

```bash
./tools/configure-dev.sh --check
./tools/configure-dev.sh --check-package
```

Os checks sao somente leitura, nao instalam pacotes e nao alteram o cache de
build.

Para um prefixo Qt fora dos caminhos detectados:

```bash
QT_DIR=/path/to/qt ./tools/configure-dev.sh
```

## Build e teste

```bash
./scripts/build-debian.sh
ctest --preset dev --output-on-failure
```

O executavel `build/dev/ssa_consulta_rapida` e um binario de desenvolvimento
dinamicamente ligado. Ele nao substitui o bundle distribuivel.

Compiladores:

| Plataforma | Compilador | Estado |
| --- | --- | --- |
| Debian/WSL amd64 | GCC 14.2 | Build e 641 testes validados |
| Debian/WSL amd64 | Clang 19.1.7 | Suportado pelo fonte, sem gate completo nesta rodada |
| Arch/Artix | GCC ou Clang C++20 | Suportado pelo fonte, sem gate nesta rodada |

## Bundle Linux portatil

```bash
./scripts/package-linux.sh
```

Gera em `dist/linux/<arch>/`:

- `ssa_consulta_rapida_cpp-linux-<arch>-<version>.tar.gz`;
- bundle extraido com launcher `ssa_consulta_rapida`, `bin/`, `lib/`, plugins e
  imports QML;
- `latest.tar.gz`, `latest-binary`, `latest-raw` e `latest-run.sh`.

Use o launcher do bundle. O binario `latest-raw` depende do ambiente de
`LD_LIBRARY_PATH` preparado pelo launcher.

## Pacote Debian

```bash
./scripts/package-debian.sh
```

Gera em `dist/linux/<arch>/`:

- `ssa_consulta_rapida_cpp-debian-<arch>-<version>.deb`;
- payload extraido para inspecao;
- `latest.deb` e `latest-binary`.

O pacote instala o launcher em `/usr/bin/ssa_consulta_rapida` e mantem as
bibliotecas da aplicacao sob `/usr/lib/ssa_consulta_rapida`. `dpkg-shlibdeps`
calcula `Depends` a partir dos ELF finais. Qt e SQLite ficam dentro do pacote;
somente bibliotecas externas do sistema entram no metadata Debian.

Saidas canonicas em `dist/linux/<arch>/final/`:

- `ssa_consulta_rapida_cpp`: executavel unico e autoextraivel;
- `ssa_consulta_rapida_cpp.deb`: pacote Debian;
- `ssa_consulta_rapida_cpp.zip`: payload portatil para extracao.

Em um `HEAD` limpo apontando exatamente para `v<version>`, o script tambem
preserva `ssa_consulta_rapida_cpp-<version>`,
`ssa_consulta_rapida_cpp-<version>.deb` e
`ssa_consulta_rapida_cpp-<version>.zip`. Builds sem tag atualizam somente os
nomes sem versao.

`./scripts/make_clean` preserva todo o diretorio `dist/`.

## Pacote Arch e Artix

```bash
./scripts/package-arch.sh
```

Gera em `dist/linux/<arch>/`:

- `ssa_consulta_rapida-<version>-<arch>-linux.pkg.tar.zst`;
- payload extraido para inspecao;
- `latest.pkg.tar.zst` e `latest-binary`.

Esse pacote requer `makepkg` e `zstd`; o comando de instalacao Arch/Artix acima
inclui `makepkg` via `base-devel`, mas `zstd` deve estar disponivel no host.

Nao inclua bancos reais, configuracoes locais, logs, screenshots ou segredos.
