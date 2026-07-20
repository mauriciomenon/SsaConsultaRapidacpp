# SSA Theme Lab

Para reproduzir o relevo glossy de `ssa-dark` em uma paleta candidata, siga
`docs/development/theme-authoring.md`. O efeito atual vem de camadas e bordas,
nao de gradiente. Nao adicione um tema ao tratamento `quietAccent` sem gate de
contraste e smoke runtime.

This developer-only tool imports a pinned Tinted Base16 catalog, maps it to
the semantic roles used by `Theme.qml`, rejects inaccessible palettes, and
produces a perceptually deduplicated shortlist. Build and runtime stay fully
offline.

Pinned inputs:

- Tinted schemes: `2ccef2f4b22e3cab5a9292811f7133a07eeba4a7`.
- Tinted builder: `9055401be7e8a4b869c780589e6f41cffb87023c`.
- Builder version/spec: `0.21.0` / `0.11.1`.

The builder is GPL-3.0-only and remains an external development executable.
It is never linked, vendored, packaged, or invoked by the application. The
scheme catalog is MIT. A selected theme is shipped only after its original
source and license are verified separately.

## Prepare pinned inputs

```sh
mkdir -p build/theme-lab/upstream
git clone https://github.com/tinted-theming/schemes.git \
  build/theme-lab/upstream/tinted-schemes
git -C build/theme-lab/upstream/tinted-schemes checkout --detach \
  2ccef2f4b22e3cab5a9292811f7133a07eeba4a7

git clone https://github.com/tinted-theming/tinted-builder-rust.git \
  build/theme-lab/upstream/tinted-builder-rust
git -C build/theme-lab/upstream/tinted-builder-rust checkout --detach \
  9055401be7e8a4b869c780589e6f41cffb87023c
git -C build/theme-lab/upstream/tinted-builder-rust apply \
  "$PWD/tools/theme_lab/tinted-builder-security.patch"
```

Do not use `tinted-builder-rust sync`: it follows a moving branch instead of
the approved commit.

## Validate and build the external builder

```sh
cd build/theme-lab/upstream/tinted-builder-rust
cargo update -p quick-xml --precise 0.41.0
cargo update -p anyhow --precise 1.0.103
cargo audit --file Cargo.lock --deny warnings
cargo test --locked --workspace
cd -
```

## Generate and score all Base16 schemes

```sh
rm -rf build/theme-lab/template
cmake -E copy_directory tools/theme_lab/tinted_template \
  build/theme-lab/template

build/theme-lab/upstream/tinted-builder-rust/target/debug/tinted-builder-rust \
  build build/theme-lab/template \
  --schemes-dir build/theme-lab/upstream/tinted-schemes --quiet

cmake --build --preset dev --target ssa_theme_lab
build/dev/ssa_theme_lab \
  --input build/theme-lab/template/generated \
  --output build/theme-lab/report.json \
  --limit 24 \
  --source-commit 2ccef2f4b22e3cab5a9292811f7133a07eeba4a7
```

All downloaded, generated, and rendered files stay below ignored `build/`.
Only a reviewed final palette and its source notice may enter the product.
The approved inputs and their SHA-256 hashes are pinned in
`third_party/tinted-themes/LOCK.json`; shipped attribution is in
`THIRD_PARTY_NOTICES.md`.
