#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  ./scripts/package-arch.sh [--preset <preset>] [--arch <arch>] [--dist-dir <dir>] [--project-root <dir>] [--version <version>] [--skip-tests]

Build and package an Arch Linux (.pkg.tar.zst) release artifact.

Defaults:
  Preset: release
  Architecture: uname -m (x86_64 or aarch64)
  Artifact dir: dist/linux/<arch>/

Parameters:
  --preset <preset>
  --arch <arch>
  --dist-dir <dir>
  --project-root <dir>
  --version <version>
  --skip-tests

Generated files:
  - ssa_consulta_rapida-<version>-<arch>-linux.pkg.tar.zst
  - latest.pkg.tar.zst points to the newest package for this architecture
  - dist/linux/<arch>/ssa_consulta_rapida-<version>-<arch>-linux/ (extracted bundle)

Requirements (Arch/Artix host):
  - makepkg, zstd (pacman package)
  - Qt6 runtime, sqlite (same as build)
EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/lib/package_common.sh"
repo_root="$(package_repo_root_from_script "${BASH_SOURCE[0]}")"
preset="release"
version=""
run_tests="true"
arch="$(uname -m)"
dist_root=""

# Capturar args originais ANTES do parser consumi-los (para re-exec como builder).
SSA_ORIGINAL_ARGS=("$@")

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --preset)
      preset="${2}"
      shift 2
      ;;
    --arch)
      arch="${2}"
      shift 2
      ;;
    --dist-dir)
      dist_root="${2}"
      shift 2
      ;;
    --project-root)
      repo_root="${2}"
      shift 2
      ;;
    --version)
      version="${2}"
      shift 2
      ;;
    --skip-tests)
      run_tests="false"
      shift
      ;;
    --help|-h)
      show_help
      exit 0
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      show_help >&2
      exit 1
      ;;
  esac
done

if [[ -z "${dist_root}" ]]; then
  dist_root="${repo_root}/dist/linux/${arch}"
fi

if [[ -z "${version}" ]]; then
  version="$(package_project_version "${repo_root}")"
fi
if [[ -z "${version}" ]]; then
  echo "Could not read project version from ${repo_root}/CMakeLists.txt. Use --version to force a value." >&2
  exit 1
fi

if ! command -v makepkg >/dev/null 2>&1; then
  echo "makepkg not found. This script must run on an Arch/Artix host." >&2
  exit 1
fi
if ! command -v zstd >/dev/null 2>&1; then
  echo "zstd not found. Install it (pacman -S zstd)." >&2
  exit 1
fi

# makepkg se recusa a rodar como root. Se somos root, re-executa este script
# como usuario builder nao-root, preservando o diretorio, os argumentos e as
# variaveis de ambiente relevantes (CMAKE_PREFIX_PATH, QT_DIR, PATH).
if [[ "$(id -u)" -eq 0 ]]; then
  if ! id builder >/dev/null 2>&1; then
    echo "Creating non-root builder user for makepkg." >&2
    useradd -m -s /bin/bash builder
  fi
  chown -R builder:builder "${repo_root}"
  # Passar args via variavel de ambiente para evitar problemas de quoting do
  # su -c "...". 'runuser -l' cria shell de login limpo, entao reexportamos
  # as vars necessarias explicitamente.
  export SSA_REEXEC_ARGS="${SSA_ORIGINAL_ARGS[*]-}"
  exec runuser -u builder -- env \
    CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH-}" \
    QT_DIR="${QT_DIR-}" \
    PATH="${PATH}" \
    SSA_REEXEC_ARGS="${SSA_REEXEC_ARGS-}" \
    bash -lc "cd $(printf '%q' "${repo_root}") && bash $(printf '%q' "${BASH_SOURCE[0]}") \${SSA_REEXEC_ARGS}"
fi

build_dir="${repo_root}/build/${preset}"
artifact_name="ssa_consulta_rapida-${version}-${arch}-linux"
artifact_root="${dist_root}/${artifact_name}"
pkg_path="${dist_root}/${artifact_name}.pkg.tar.zst"

"${repo_root}/tools/configure-dev.sh" "${preset}"
cmake --build --preset "${preset}"
if [[ "${run_tests}" == "true" ]]; then
  ctest --preset "${preset}" --output-on-failure
fi

binary="${build_dir}/ssa_consulta_rapida"
if [[ ! -x "${binary}" ]]; then
  echo "Binary not found: ${binary}" >&2
  exit 1
fi

# Build via a temporary PKGBUILD using makepkg, pointing at the freshly built
# binary so we produce a self-contained package without rebuilding from source.
pkgbuild_root="${build_dir}/_arch_stage"
rm -rf "${pkgbuild_root}"
mkdir -p "${pkgbuild_root}/src"
cp "${binary}" "${pkgbuild_root}/src/ssa_consulta_rapida"
package_copy_runtime_libraries "${binary}" "${pkgbuild_root}/src"
# Deploy de plugins Qt + imports QML (bundle autocontido).
package_copy_qt_resources "${binary}" "${pkgbuild_root}/src"

cat > "${pkgbuild_root}/PKGBUILD" <<EOF_PKGBUILD
# Maintainer: Mauricio Menon <mauriciomenon@users.noreply.github.com>
pkgname=ssa-consulta-rapida
pkgver=${version}
pkgrel=1
epoch=
pkgdesc="Consulta rapida de SSAs (ordens de servico)"
arch=('${arch}')
url="https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp"
license=('MIT')
# Bundle autocontido em Qt/QML; depende das libs de sistema de display/font
# que os plugins Qt (xcb/wayland) carregam dinamicamente.
depends=('glibc' 'sqlite' 'libgl' 'libegl' 'libxkbcommon' 'dbus' 'fontconfig' 'freetype2')
makedepends=()
checkdepends=()
optdepends=()
provides=('ssa-consulta-rapida')
conflicts=()
replaces=()
backup=()
options=()
install=
changelog=
source=()
noextract=()
sha256sums=()
validpgpkeys=()

build() {
  return 0
}

package() {
  install -Dm0755 "\${srcdir}/ssa_consulta_rapida" "\${pkgdir}/usr/lib/ssa_consulta_rapida/bin/ssa_consulta_rapida"
  for lib in "\${srcdir}"/lib*.so* "\${srcdir}"/*.so.*; do
    [[ -e "\${lib}" ]] || continue
    install -Dm0755 "\${lib}" "\${pkgdir}/usr/lib/ssa_consulta_rapida/lib/\$(basename "\${lib}")"
  done
  if [[ -d "\${srcdir}/plugins" ]]; then
    cp -R "\${srcdir}/plugins" "\${pkgdir}/usr/lib/ssa_consulta_rapida/plugins"
  fi
  if [[ -d "\${srcdir}/qml" ]]; then
    cp -R "\${srcdir}/qml" "\${pkgdir}/usr/lib/ssa_consulta_rapida/qml"
  fi
  install -d "\${pkgdir}/usr/bin"
  cat > "\${pkgdir}/usr/bin/ssa_consulta_rapida" <<'LAUNCHER'
#!/usr/bin/env bash
set -euo pipefail
APP_DIR="/usr/lib/ssa_consulta_rapida"
export LD_LIBRARY_PATH="\${APP_DIR}/lib\${LD_LIBRARY_PATH+:}\${LD_LIBRARY_PATH-}"
[[ -d "\${APP_DIR}/plugins" ]] && export QT_PLUGIN_PATH="\${APP_DIR}/plugins"
[[ -d "\${APP_DIR}/qml" ]] && export QML_IMPORT_PATH="\${APP_DIR}/qml" QML2_IMPORT_PATH="\${APP_DIR}/qml"
exec "\${APP_DIR}/bin/ssa_consulta_rapida" "\$@"
LAUNCHER
  chmod 0755 "\${pkgdir}/usr/bin/ssa_consulta_rapida"
  install -d "\${pkgdir}/usr/share/applications"
  cat > "\${pkgdir}/usr/share/applications/ssa-consulta-rapida.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SSA Consulta Rapida
Comment=Consulta rapida de SSAs
Exec=ssa_consulta_rapida
Categories=Utility;Office;
Terminal=false
DESKTOP
}
EOF_PKGBUILD

(
  cd "${pkgbuild_root}"
  makepkg --syncdeps --noconfirm --skipinteg --skippgpcheck \
          --force --noextract --nocolor
)

# Locate the produced package inside the staging dir.
produced="$(find "${pkgbuild_root}" -maxdepth 1 -name 'ssa-consulta-rapida-*.pkg.tar.zst' -print -quit || true)"
if [[ -z "${produced}" || ! -f "${produced}" ]]; then
  echo "makepkg did not produce a package under ${pkgbuild_root}." >&2
  exit 1
fi

mkdir -p "${dist_root}"
rm -f "${pkg_path}"
cp "${produced}" "${pkg_path}"

# Keep an extracted copy of the installed tree for convenience.
rm -rf "${artifact_root}"
mkdir -p "${artifact_root}"
tar -C "${artifact_root}" -xf "${pkg_path}" usr share 2>/dev/null || \
  tar -C "${artifact_root}" -xf "${pkg_path}"

package_set_latest_link "${dist_root}" "${artifact_name}"
package_set_latest_alias "${dist_root}" "latest.pkg.tar.zst" "${artifact_name}.pkg.tar.zst"
package_set_latest_alias "${dist_root}" "latest-binary" \
  "${artifact_name}/usr/bin/ssa_consulta_rapida"

rm -rf "${pkgbuild_root}"

cat <<EOF_REPORT
Arch release artifacts generated:
  project_root: ${repo_root}
  version: ${version}
  preset: ${preset}
  architecture: ${arch}
  package: ${pkg_path}
  artifact_root: ${artifact_root}
  latest_pkg: ${dist_root}/latest.pkg.tar.zst
  latest_binary: ${dist_root}/latest-binary
  latest: ${dist_root}/latest
EOF_REPORT
