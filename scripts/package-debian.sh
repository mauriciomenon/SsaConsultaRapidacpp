#!/usr/bin/env bash
set -euo pipefail

# Do not inherit Windows temporary directories when invoked from WSL.
export TMPDIR=/tmp
export TMP=/tmp
export TEMP=/tmp

show_help() {
  cat <<'EOF'
Usage:
  ./scripts/package-debian.sh [--preset <preset>] [--arch <arch>] [--dist-dir <dir>] [--project-root <dir>] [--version <version>] [--skip-tests]

Build and package a Debian (.deb) release artifact.

Defaults:
  Preset: release
  Architecture: dpkg --print-architecture (fallback to uname -m)
  Artifact dir: dist/linux/<arch>/gcc/

Parameters:
  --preset <preset>
  --arch <arch>
  --dist-dir <dir>
  --project-root <dir>
  --version <version>
  --skip-tests

Generated files:
  - final/<repo>-<version>-<commit>-debian-<arch>-gcc (single self-extracting executable)
  - final/<repo>-<version>-<commit>-debian-<arch>-gcc-standalone/<repo>-<version>-<commit>-debian-<arch>-gcc
  - final/<repo>-<version>-<commit>-debian-<arch>-gcc.deb
  - final/<repo>-<version>-<commit>-debian-<arch>-gcc.zip
  - releases/<version>-<commit>-debian-<arch>-gcc contains immutable delivery sets
  - current.json identifies the current complete release

Requirements (Debian/Ubuntu host):
  - dpkg-deb and dpkg-shlibdeps; fakeroot is optional
  - file, zip and tar
  - Qt6 dev and runtime, sqlite3, cmake, ninja (same as build)
EOF
}

require_option_value() {
  local option="$1"
  local value="${2-}"
  if [[ -z "${value}" || "${value}" == --* ]]; then
    echo "${option} requires a value." >&2
    show_help >&2
    exit 1
  fi
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./scripts/lib/package_common.sh
source "${script_dir}/lib/package_common.sh"
repo_root="$(package_repo_root_from_script "${BASH_SOURCE[0]}")"
preset="release"
version=""
run_tests="true"
arch="$(package_linux_arch)"
dist_root=""

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --preset)
      require_option_value "${1}" "${2-}"
      preset="${2}"
      shift 2
      ;;
    --arch)
      require_option_value "${1}" "${2-}"
      arch="${2}"
      shift 2
      ;;
    --dist-dir)
      require_option_value "${1}" "${2-}"
      dist_root="${2}"
      shift 2
      ;;
    --project-root)
      require_option_value "${1}" "${2-}"
      repo_root="${2}"
      shift 2
      ;;
    --version)
      require_option_value "${1}" "${2-}"
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
  dist_root="${repo_root}/dist/linux/${arch}/gcc"
fi

if [[ -z "${version}" ]]; then
  version="$(package_project_version "${repo_root}")"
fi
if [[ -z "${version}" ]]; then
  echo "Could not read project version from ${repo_root}/CMakeLists.txt. Use --version to force a value." >&2
  exit 1
fi

build_dir="${repo_root}/build/${preset}"
repo_name="$(package_repo_name "${repo_root}")"
toolchain="gcc"
commit_sha="$(git -C "${repo_root}" rev-parse --short=12 HEAD)"
artifact_name="${repo_name}-${version}-${commit_sha}-debian-${arch}-${toolchain}"
final_root="${dist_root}/final"
stage_root="$(mktemp -d "${TMPDIR:-/tmp}/${artifact_name}.XXXXXX")"
cleanup_package() {
  rm -rf "${stage_root}" "${build_dir}"
}
trap cleanup_package EXIT
trap 'exit 1' HUP INT TERM
stage_artifact_root="${stage_root}/${artifact_name}"
release_stage="${stage_root}/release"
stage_deb_path="${release_stage}/${artifact_name}.deb"
stage_direct_path="${release_stage}/${artifact_name}"
stage_zip_path="${release_stage}/${artifact_name}.zip"
stage_standalone_root="${release_stage}/${artifact_name}-standalone"

# Map uname arch to Debian arch when running on non-dpkg hosts is not needed:
# this script is meant to run on a Debian/Ubuntu host where dpkg exists.
if ! command -v dpkg-deb >/dev/null 2>&1; then
  echo "dpkg-deb not found. This script must run on a Debian/Ubuntu host." >&2
  exit 1
fi
for required_tool in dpkg-shlibdeps file objdump zip tar sha256sum; do
  if ! command -v "${required_tool}" >/dev/null 2>&1; then
    echo "${required_tool} not found. Install the Debian packaging prerequisites." >&2
    exit 1
  fi
done

rm -rf "${build_dir}"
"${repo_root}/tools/configure-dev.sh" "${preset}"
(cd "${repo_root}"
if [[ "${run_tests}" == "true" ]]; then
  cmake --build --preset "${preset}"
  ctest --preset "${preset}" --output-on-failure
else
  cmake --build --preset "${preset}" --target ssa_consulta_rapida \
    ssa_consulta_rapida_cli
fi)

build_binary="${build_dir}/ssa_consulta_rapida"
if [[ ! -x "${build_binary}" ]]; then
  echo "Binary not found: ${build_binary}" >&2
  exit 1
fi

cmake_install_root="${stage_root}/cmake-install"
cmake --install "${build_dir}" --prefix "${cmake_install_root}"
packaged_binary="${cmake_install_root}/bin/ssa_consulta_rapida"
if [[ ! -x "${packaged_binary}" ]]; then
  echo "Installed binary not found: ${packaged_binary}" >&2
  exit 1
fi
expected_runpath="\$ORIGIN/../lib:\$ORIGIN/lib"
packaged_runpath="$(objdump -p "${packaged_binary}" |
  awk '$1 == "RUNPATH" { print $2; exit }')"
if [[ "${packaged_runpath}" != "${expected_runpath}" ]]; then
  echo "Packaged binary has unsafe RUNPATH: ${packaged_runpath:-<empty>}" >&2
  exit 1
fi

# Stage the .deb payload under the classic Debian install layout.
pkgroot="${stage_root}/pkgroot"
install_prefix="${pkgroot}/usr"
mkdir -p "${install_prefix}/lib/ssa_consulta_rapida/bin" \
         "${install_prefix}/lib/ssa_consulta_rapida/lib" \
         "${install_prefix}/bin" \
         "${install_prefix}/share/applications" \
         "${install_prefix}/share/icons/hicolor/512x512/apps" \
         "${install_prefix}/share/icons/hicolor/scalable/apps" \
         "${install_prefix}/share/doc/ssa-consulta-rapida"

cp "${packaged_binary}" \
  "${install_prefix}/lib/ssa_consulta_rapida/bin/ssa_consulta_rapida"
chmod 0755 "${install_prefix}/lib/ssa_consulta_rapida/bin/ssa_consulta_rapida"
cp "${repo_root}/THIRD_PARTY_NOTICES.md" \
  "${install_prefix}/share/doc/ssa-consulta-rapida/"
cp "${repo_root}/third_party/tinted-themes/LICENSE" \
  "${install_prefix}/share/doc/ssa-consulta-rapida/TINTED_SCHEMES_LICENSE.txt"
package_copy_runtime_libraries "${build_binary}" \
  "${install_prefix}/lib/ssa_consulta_rapida/lib"
# Deploy de plugins Qt + imports QML para o bundle ser autocontido.
package_copy_qt_resources "${build_binary}" "${install_prefix}/lib/ssa_consulta_rapida"
cp "${repo_root}/resources/app_icon.png" \
   "${install_prefix}/share/icons/hicolor/512x512/apps/ssa-consulta-rapida.png"
cp "${repo_root}/resources/app_icon.svg" \
   "${install_prefix}/share/icons/hicolor/scalable/apps/ssa-consulta-rapida.svg"
cp "${repo_root}/resources/ssa-consulta-rapida.desktop" \
   "${install_prefix}/share/applications/ssa-consulta-rapida.desktop"

cat > "${install_prefix}/lib/ssa_consulta_rapida/ssa_consulta_rapida" <<'EOF_LAUNCHER'
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "${SCRIPT_PATH}")" && pwd)"
if [[ -n "${LD_LIBRARY_PATH-}" ]]; then
  export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib"
fi
if [[ -d "${SCRIPT_DIR}/plugins" ]]; then
  export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins"
fi
if [[ -d "${SCRIPT_DIR}/qml" ]]; then
  if [[ -n "${QML_IMPORT_PATH-}" ]]; then
    export QML_IMPORT_PATH="${SCRIPT_DIR}/qml:${QML_IMPORT_PATH}"
    export QML2_IMPORT_PATH="${SCRIPT_DIR}/qml:${QML2_IMPORT_PATH-}"
  else
    export QML_IMPORT_PATH="${SCRIPT_DIR}/qml"
    export QML2_IMPORT_PATH="${SCRIPT_DIR}/qml"
  fi
fi
exec "${SCRIPT_DIR}/bin/ssa_consulta_rapida" "$@"
EOF_LAUNCHER
chmod 0755 "${install_prefix}/lib/ssa_consulta_rapida/ssa_consulta_rapida"

ln -sf "../lib/ssa_consulta_rapida/ssa_consulta_rapida" \
       "${install_prefix}/bin/ssa_consulta_rapida"

cat > "${install_prefix}/share/doc/ssa-consulta-rapida/copyright" <<EOF_COPYRIGHT
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: SSA Consulta Rapida Cpp
Source: https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp

Files: *
Copyright: 2024-2026 Mauricio Menon
License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
EOF_COPYRIGHT

printf 'ssa-consulta-rapida (%s) stable; urgency=low\n\n  * Release %s.\n\n -- Mauricio Menon <mauriciomenon@users.noreply.github.com>  %s\n' \
  "${version}" "${version}" "$(date -R)" \
  | gzip -n > "${install_prefix}/share/doc/ssa-consulta-rapida/changelog.Debian.gz"

maintainer="Mauricio Menon <mauriciomenon@users.noreply.github.com>"
installed_size_kb="$(du -sk "${install_prefix}" | awk '{print $1}')"

mkdir -p "${pkgroot}/DEBIAN"
mkdir -p "${stage_root}/debian"
cat > "${stage_root}/debian/control" <<EOF_SHLIB_CONTROL
Source: ssa-consulta-rapida
Section: utils
Priority: optional
Maintainer: ${maintainer}
Standards-Version: 4.6.2

Package: ssa-consulta-rapida
Architecture: any
Description: Consulta rapida de SSAs
EOF_SHLIB_CONTROL

mapfile -d '' elf_files < <(
  find "${install_prefix}/lib/ssa_consulta_rapida" -type f -print0 |
    while IFS= read -r -d '' candidate; do
      if file -Lb "${candidate}" |
        grep -Eq '^ELF .* (executable|shared object),'; then
        printf '%s\0' "${candidate}"
      fi
    done
)
mapfile -d '' bundled_shared_libraries < <(
  find "${install_prefix}/lib/ssa_consulta_rapida" -type f -name '*.so*' \
    -print0
)
printf '' > "${stage_root}/debian/shlibs.local"
for bundled_library in "${bundled_shared_libraries[@]}"; do
  soname="$(objdump -p "${bundled_library}" |
    awk '$1 == "SONAME" { value = $2 } END { print value }')"
  if [[ "${soname}" =~ ^(.+)\.so\.([0-9]+)(\..*)?$ ]]; then
    printf '%s %s ssa-consulta-rapida (= %s)\n' \
      "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${version}" \
      >> "${stage_root}/debian/shlibs.local"
  fi
done
sort -u -o "${stage_root}/debian/shlibs.local" \
  "${stage_root}/debian/shlibs.local"

mapfile -d '' private_lib_dirs < <(
  find "${install_prefix}/lib/ssa_consulta_rapida" -type f -name '*.so*' \
    -printf '%h\0' | sort -zu
)
shlib_args=()
for private_lib_dir in "${private_lib_dirs[@]}"; do
  shlib_args+=("-l${private_lib_dir}")
done
for elf_file in "${elf_files[@]}"; do
  shlib_args+=("-e${elf_file}")
done
runtime_depends="$(
  cd "${stage_root}"
  dpkg-shlibdeps -O --package=ssa-consulta-rapida \
    -xssa-consulta-rapida "${shlib_args[@]}" |
    sed -n 's/^shlibs:Depends=//p'
)"
if [[ -z "${runtime_depends}" ]]; then
  echo "dpkg-shlibdeps did not produce runtime dependencies." >&2
  exit 1
fi

cat > "${pkgroot}/DEBIAN/control" <<EOF_CONTROL
Package: ssa-consulta-rapida
Version: ${version}
Section: utils
Priority: optional
Architecture: ${arch}
Installed-Size: ${installed_size_kb}
Maintainer: ${maintainer}
Description: Consulta rapida de SSAs (ordens de servico)
 Aplicacao desktop C++/Qt6/QML para consulta de SSAs, com filtros,
 exportacao e detalhes. Bundle autossuficiente com bibliotecas Qt
 incluidas quando possivel.
Depends: ${runtime_depends}
EOF_CONTROL

cat > "${pkgroot}/DEBIAN/postinst" <<'EOF_POSTINST'
#!/usr/bin/env bash
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q "/usr/share/applications" || true
fi
exit 0
EOF_POSTINST
chmod 0755 "${pkgroot}/DEBIAN/postinst"

mkdir -p "${release_stage}"
if command -v fakeroot >/dev/null 2>&1; then
  fakeroot dpkg-deb --build --root-owner-group "${pkgroot}" "${stage_deb_path}"
else
  dpkg-deb --build --root-owner-group "${pkgroot}" "${stage_deb_path}"
fi

mkdir -p "${stage_artifact_root}"
cp -R "${install_prefix}" "${stage_artifact_root}/usr"
cp "${pkgroot}/DEBIAN/control" "${stage_artifact_root}/control"
package_create_linux_direct_executable \
  "${stage_artifact_root}" "${stage_direct_path}"
(
  cd "${stage_root}"
  zip -qr "${stage_zip_path}" "${artifact_name}"
)

mkdir -p "${stage_standalone_root}/lib"
cp "${install_prefix}/lib/ssa_consulta_rapida/bin/ssa_consulta_rapida" \
  "${stage_standalone_root}/${artifact_name}"
cp -R "${install_prefix}/lib/ssa_consulta_rapida/lib/." "${stage_standalone_root}/lib/"
for runtime_dir in plugins qml; do
  if [[ -d "${install_prefix}/lib/ssa_consulta_rapida/${runtime_dir}" ]]; then
    cp -R "${install_prefix}/lib/ssa_consulta_rapida/${runtime_dir}" "${stage_standalone_root}/"
  fi
done
chmod 0755 "${stage_standalone_root}/${artifact_name}"
cat > "${stage_standalone_root}/README.txt" <<EOF_STANDALONE
Native Linux executable. Run:
  ./${artifact_name} --db <path-to-ssas.db>

The lib/, plugins/ and qml/ directories must remain beside the executable.
EOF_STANDALONE

required_release_paths=(
  "${artifact_name}"
  "${artifact_name}.deb"
  "${artifact_name}.zip"
  "${artifact_name}-standalone/${artifact_name}"
)
cmake_cache="${build_dir}/CMakeCache.txt"
qt_kit="$(sed -n 's/^Qt6_DIR:[^=]*=//p' "${cmake_cache}" | head -n 1)"
compiler="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "${cmake_cache}" | head -n 1)"
linker="$(sed -n 's/^CMAKE_LINKER:[^=]*=//p' "${cmake_cache}" | head -n 1)"
if [[ -z "${qt_kit}" || -z "${compiler}" || -z "${linker}" || ! -x "${compiler}" || ! -x "${linker}" ]]; then
  echo "CMake cache is missing Qt6_DIR, an effective compiler, or an effective linker." >&2
  exit 1
fi
compiler_version="$("${compiler}" --version | head -n 1)"
linker_version="$("${linker}" --version | head -n 1)"
tagged_release="false"
if package_is_exact_release_tag "${repo_root}" "${version}"; then
  tagged_release="true"
fi
package_publish_release_set "${release_stage}" "${dist_root}" "${version}" \
  "${commit_sha}" "debian" "${arch}" "${toolchain}" "${preset}" "${qt_kit}" "${repo_root}" \
  "${compiler}" "${compiler_version}" "${linker}" "${linker_version}" "${tagged_release}" \
  "${required_release_paths[@]}"

cleanup_package
trap - EXIT HUP INT TERM

release_report=""
if [[ "${tagged_release}" == "true" ]]; then
  release_report="  release: ${dist_root}/releases/${version}-${commit_sha}-debian-${arch}-${toolchain}"
fi
cat <<EOF_REPORT
Debian release artifacts generated:
  project_root: ${repo_root}
  version: ${version}
  preset: ${preset}
  architecture: ${arch}
  toolchain: ${toolchain}
  package: ${final_root}/${artifact_name}.zip
  executable: ${final_root}/${artifact_name}
  standalone: ${final_root}/${artifact_name}-standalone/${artifact_name}
  deb: ${final_root}/${artifact_name}.deb
  final_root: ${final_root}
${release_report}
  current: ${dist_root}/current.json
  dependencies: ${runtime_depends}
  build_dir_removed: ${build_dir}
EOF_REPORT
