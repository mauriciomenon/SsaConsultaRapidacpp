#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  ./scripts/package-debian.sh [--preset <preset>] [--arch <arch>] [--dist-dir <dir>] [--project-root <dir>] [--version <version>] [--skip-tests]

Build and package a Debian (.deb) release artifact.

Defaults:
  Preset: release
  Architecture: dpkg --print-architecture (fallback to uname -m)
  Artifact dir: dist/linux/<arch>/

Parameters:
  --preset <preset>
  --arch <arch>
  --dist-dir <dir>
  --project-root <dir>
  --version <version>
  --skip-tests

Generated files:
  - ssa_consulta_rapida-<version>-<arch>-linux.deb
  - latest.deb points to the newest .deb for this architecture
  - dist/linux/<arch>/ssa_consulta_rapida-<version>-<arch>-linux/ (extracted bundle)

Requirements (Debian/Ubuntu host):
  - dpkg-deb, fakeroot (dpkg-dev package)
  - Qt6 dev and runtime, sqlite3, cmake, ninja (same as build)
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
arch="$(package_linux_arch)"
dist_root=""

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

build_dir="${repo_root}/build/${preset}"
artifact_name="ssa_consulta_rapida-${version}-${arch}-linux"
artifact_root="${dist_root}/${artifact_name}"
deb_path="${dist_root}/${artifact_name}.deb"

# Map uname arch to Debian arch when running on non-dpkg hosts is not needed:
# this script is meant to run on a Debian/Ubuntu host where dpkg exists.
if ! command -v dpkg-deb >/dev/null 2>&1; then
  echo "dpkg-deb not found. This script must run on a Debian/Ubuntu host." >&2
  exit 1
fi

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

# Stage the .deb payload under the classic Debian install layout.
pkgroot="${build_dir}/_deb_stage/${artifact_name}"
rm -rf "${pkgroot}"
install_prefix="${pkgroot}/usr"
mkdir -p "${install_prefix}/lib/ssa_consulta_rapida/bin" \
         "${install_prefix}/lib/ssa_consulta_rapida/lib" \
         "${install_prefix}/bin" \
         "${install_prefix}/share/applications" \
         "${install_prefix}/share/doc/ssa-consulta-rapida" \
         "${install_prefix}/share/icons/hicolor/256x256/apps"

cp "${binary}" "${install_prefix}/lib/ssa_consulta_rapida/bin/ssa_consulta_rapida"
chmod 0755 "${install_prefix}/lib/ssa_consulta_rapida/bin/ssa_consulta_rapida"
package_copy_runtime_libraries "${binary}" "${install_prefix}/lib/ssa_consulta_rapida/lib"

cat > "${install_prefix}/lib/ssa_consulta_rapida/ssa_consulta_rapida" <<'EOF_LAUNCHER'
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -n "${LD_LIBRARY_PATH-}" ]]; then
  export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib"
fi
exec "${SCRIPT_DIR}/bin/ssa_consulta_rapida" "$@"
EOF_LAUNCHER
chmod 0755 "${install_prefix}/lib/ssa_consulta_rapida/ssa_consulta_rapida"

ln -sf "../lib/ssa_consulta_rapida/ssa_consulta_rapida" \
       "${install_prefix}/bin/ssa_consulta_rapida"

cat > "${install_prefix}/share/applications/ssa-consulta-rapida.desktop" <<EOF_DESKTOP
[Desktop Entry]
Type=Application
Name=SSA Consulta Rapida
Comment=Consulta rapida de SSAs
Exec=ssa_consulta_rapida
Icon=ssa-consulta-rapida
Categories=Utility;Office;
Terminal=false
EOF_DESKTOP

# Minimal placeholder icon so the desktop entry resolves.
printf '\x89PNG\r\n\x1a\n' > "${install_prefix}/share/icons/hicolor/256x256/apps/ssa-consulta-rapida.png"

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

cat > "${install_prefix}/share/doc/ssa-consulta-rapida/changelog.Debian.gz" <<EOF_CHANGELOG
placeholder changelog for ssa-consulta-rapida ${version}; see upstream releases.
EOF_CHANGELOG

maintainer="Mauricio Menon <mauriciomenon@users.noreply.github.com>"
installed_size_kb="$(du -sk "${install_prefix}" | awk '{print $1}')"

mkdir -p "${pkgroot}/DEBIAN"
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
Depends: libc6, libsqlite3-0
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

mkdir -p "${dist_root}"
rm -f "${deb_path}"
if command -v fakeroot >/dev/null 2>&1; then
  fakeroot dpkg-deb --build --root-owner-group "${pkgroot}" "${deb_path}"
else
  dpkg-deb --build --root-owner-group "${pkgroot}" "${deb_path}"
fi

# Keep an extracted copy under dist/linux/<arch>/ for convenience.
rm -rf "${artifact_root}"
mkdir -p "${artifact_root}"
cp -R "${install_prefix}" "${artifact_root}/usr"
cp "${pkgroot}/DEBIAN/control" "${artifact_root}/control"

package_set_latest_link "${dist_root}" "${artifact_name}"
package_set_latest_alias "${dist_root}" "latest.deb" "${artifact_name}.deb"
package_set_latest_alias "${dist_root}" "latest-binary" \
  "${artifact_name}/usr/lib/ssa_consulta_rapida/bin/ssa_consulta_rapida"

rm -rf "${pkgroot}"

cat <<EOF_REPORT
Debian release artifacts generated:
  project_root: ${repo_root}
  version: ${version}
  preset: ${preset}
  architecture: ${arch}
  deb: ${deb_path}
  artifact_root: ${artifact_root}
  latest_deb: ${dist_root}/latest.deb
  latest_binary: ${dist_root}/latest-binary
  latest: ${dist_root}/latest
EOF_REPORT
