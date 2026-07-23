# Build And Packaging - macOS

Alvo historicamente validado: macOS arm64 com Apple Clang e Qt 6.11.x `macos`.
Esta rodada Windows/Linux nao executou gate em um host macOS.

## Dependencias obrigatorias

Instale Xcode Command Line Tools:

```bash
xcode-select --install
```

Instale CMake, Ninja e SQLite com Homebrew:

```bash
brew install cmake ninja sqlite
```

Homebrew: https://docs.brew.sh/Installation

Instale Qt 6.11.x `macos` pelo Qt Online Installer, selecionando Qt Base e Qt
Declarative:
https://doc.qt.io/qt-6/get-and-install-qt.html

O caminho convencional e `~/Qt/<versao>/macos`. A formula Homebrew `qt` tambem
pode ser usada quando estiver na familia 6.11.x:

```bash
brew install qt
```

Prefixes Homebrew convencionais:

- Apple Silicon: `/opt/homebrew/opt/qt`;
- Intel: `/usr/local/opt/qt`.

As ferramentas Apple fornecem `clang++`, `lipo`, `codesign`, `ditto` e
`hdiutil`. O kit Qt fornece `macdeployqt`. WebEngine nao e necessario.

## Preflight

```bash
./tools/configure-dev.sh --check
./tools/configure-dev.sh --check-package
```

Os checks sao somente leitura, nao instalam pacotes e nao alteram o cache de
build.

Configuracao explicita, quando necessaria:

```bash
QT_DIR=/opt/homebrew/opt/qt ./tools/configure-dev.sh
```

## Build e teste

```bash
./scripts/build-macos.sh
ctest --preset dev --output-on-failure
```

Executaveis:

- GUI: `build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida`;
- CLI: `build/dev/ssa_consulta_rapida_cli`.

O compilador canonico e Apple Clang. Homebrew LLVM pode ser usado para analise,
mas nao substitui o gate Apple Clang sem uma validacao completa contra o SDK.

## Pacote

```bash
./scripts/package-macos.sh
```

O script configura e compila `release`, executa CTest, chama `macdeployqt`,
assina ad-hoc por padrao e gera em `dist/macos/<arch>/`:

- `ssa_consulta_rapida_cpp-macos-<arch>-<version>.zip`;
- `ssa_consulta_rapida_cpp-macos-<arch>-<version>.dmg`;
- bundle `.app` persistente;
- aliases `latest.zip`, `latest.dmg`, `latest.app`, `latest-binary` e
  `latest-run.sh`.

Saidas canonicas em `dist/macos/<arch>/final/`:

- `ssa_consulta_rapida_cpp.app`;
- `ssa_consulta_rapida_cpp.dmg`;
- `ssa_consulta_rapida_cpp.zip`.

Em um `HEAD` limpo apontando exatamente para `v<version>`, o script tambem
preserva `ssa_consulta_rapida_cpp-<version>.app`,
`ssa_consulta_rapida_cpp-<version>.dmg` e
`ssa_consulta_rapida_cpp-<version>.zip`. Builds sem tag atualizam somente os
nomes sem versao. `./scripts/make_clean` preserva `dist/`.

Para assinatura de distribuicao, informe `SSA_MACOS_CODESIGN_IDENTITY`. A
assinatura ad-hoc valida integridade local, mas nao substitui Developer ID e
notarizacao.

Antes de publicar, confirme que o preset release foi configurado e compilado,
o CTest passou, o app abriu em runtime gravavel, o smoke QML usou uma fixture e
o ZIP/DMG vieram do `.app` novo. `package-macos.sh` executa configuracao,
build, CTest, deploy, assinatura e geracao de artefatos, mas nao abre o app nem
executa o smoke com fixture. Rode separadamente antes de publicar:

```bash
./run-macos-smoke-clean
```

Nao inclua bancos reais, configuracoes locais, logs, screenshots ou segredos.
