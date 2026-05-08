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

## Build

```bash
cmake --preset dev -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Execucao

```bash
./build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida \
  --db /Users/menon/git/SSA_Consulta_Rapida/data/ssas.db
```

Em Linux/Windows, use o binario gerado no diretorio de build equivalente.

## Principios

- GUI/QML so visual.
- ViewModels Qt so coordenam estado visual e comandos.
- Query e dominio nao dependem de Qt.
- SQLite e paginado, com filtros compilados para SQL.
- Preferencias sao contrato versionado.
- Nenhum fallback silencioso.

