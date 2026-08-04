#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  ./scripts/package-linux.sh [--preset <preset>] [--arch <arch>] [--dist-dir <dir>] [--project-root <dir>] [--version <version>] [--skip-tests]

Build and package Linux release artifacts with no arguments.

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
  - final/<repo-name> (single self-extracting executable)
  - final/<repo-name>.zip
  - versioned copies are created only from a clean matching Git tag
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
# shellcheck source=scripts/lib/native_host_guard.sh
source "${script_dir}/lib/native_host_guard.sh"
entry_repo_root="$(cd "${script_dir}/.." && pwd -P)"
ssa_native_guard_repo "$entry_repo_root" || exit 1
if command -v dpkg >/dev/null 2>&1; then
  ssa_native_guard_tool dpkg || exit 1
else
  ssa_native_guard_tool uname || exit 1
fi
# shellcheck source=scripts/lib/package_common.sh
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

ssa_native_guard_repo "$repo_root" || exit 1
ssa_native_guard_tools cmake ctest git rm mkdir cp chmod tar || exit 1

if [[ -z "${dist_root}" ]]; then
  dist_root="${repo_root}/dist/linux/${arch}"
fi
ssa_native_guard_path "$dist_root" "$repo_root" || exit 1

if [[ -z "${version}" ]]; then
  version="$(package_project_version "${repo_root}")"
fi
if [[ -z "${version}" ]]; then
  echo "Could not read project version from ${repo_root}/CMakeLists.txt. Use --version to force a value." >&2
  exit 1
fi

build_dir="${repo_root}/build/${preset}"
repo_name="$(package_repo_name "${repo_root}")"
artifact_name="${repo_name}-linux-${arch}-${version}"
artifact_root="${dist_root}/${artifact_name}"
archive_path="${dist_root}/${artifact_name}.tar.gz"
final_root="${dist_root}/final"
direct_stage="${dist_root}/.${repo_name}.$$.direct"
zip_stage="${dist_root}/.${repo_name}.$$.zip"

"${repo_root}/tools/configure-dev.sh" "${preset}"
(cd "${repo_root}"
if [[ "${run_tests}" == "true" ]]; then
  cmake --build --preset "${preset}"
  ctest --preset "${preset}" --output-on-failure
else
  cmake --build --preset "${preset}" --target ssa_consulta_rapida
fi)

binary="${build_dir}/ssa_consulta_rapida"
if [[ ! -x "${binary}" ]]; then
  echo "Binary not found: ${binary}" >&2
  exit 1
fi

rm -rf "${artifact_root}"
mkdir -p "${artifact_root}/bin" "${artifact_root}/lib" \
         "${artifact_root}/share/applications" \
         "${artifact_root}/share/icons/hicolor/512x512/apps" \
         "${artifact_root}/share/icons/hicolor/scalable/apps"
rm -f "${archive_path}"
cp "${binary}" "${artifact_root}/bin/"
chmod +x "${artifact_root}/bin/ssa_consulta_rapida"
cp "${repo_root}/resources/app_icon.png" \
   "${artifact_root}/share/icons/hicolor/512x512/apps/ssa-consulta-rapida.png"
cp "${repo_root}/resources/app_icon.svg" \
   "${artifact_root}/share/icons/hicolor/scalable/apps/ssa-consulta-rapida.svg"
cp "${repo_root}/resources/ssa-consulta-rapida.desktop" \
   "${artifact_root}/share/applications/ssa-consulta-rapida.desktop"

package_copy_runtime_libraries "${binary}" "${artifact_root}/lib"
# Deploy de plugins Qt + imports QML para o bundle ser autocontido.
package_copy_qt_resources "${binary}" "${artifact_root}"
package_set_latest_link "${dist_root}" "${artifact_name}"
package_set_latest_alias "${dist_root}" "latest.tar.gz" "${artifact_name}.tar.gz"

cat > "${artifact_root}/ssa_consulta_rapida" <<'EOF_RUN'
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
"${SCRIPT_DIR}/bin/ssa_consulta_rapida" "$@"
EOF_RUN
chmod +x "${artifact_root}/ssa_consulta_rapida"
cat > "${artifact_root}/run-ssa_consulta_rapida.sh" <<'EOF_RUN_ALIAS'
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${SCRIPT_DIR}/ssa_consulta_rapida" "$@"
EOF_RUN_ALIAS
chmod +x "${artifact_root}/run-ssa_consulta_rapida.sh"
package_set_latest_alias "${dist_root}" "latest-binary" "${artifact_name}/ssa_consulta_rapida"
package_set_latest_alias "${dist_root}" "latest-raw" "${artifact_name}/bin/ssa_consulta_rapida"
package_set_latest_alias "${dist_root}" "latest-run.sh" "${artifact_name}/run-ssa_consulta_rapida.sh"

cat > "${artifact_root}/README.txt" <<EOF_README
Self sufficient bundle for Linux.
This package keeps project binary and runtime libraries, excluding system libraries.

Run:
  ./ssa_consulta_rapida --db <path-to-ssas.db>

Binary:
  bin/ssa_consulta_rapida
EOF_README

tar -czf "${archive_path}" -C "${dist_root}" "${artifact_name}"
package_create_linux_direct_executable "${artifact_root}" "${direct_stage}"
(
  cd "${dist_root}"
  zip -qr "${zip_stage}" "${artifact_name}"
)

tagged_release="false"
if package_is_exact_release_tag "${repo_root}" "${version}"; then
  tagged_release="true"
fi
package_publish_final_artifact \
  "${direct_stage}" "${final_root}" "${repo_name}" \
  "${repo_name}-${version}" "${tagged_release}"
package_publish_final_artifact \
  "${zip_stage}" "${final_root}" "${repo_name}.zip" \
  "${repo_name}-${version}.zip" "${tagged_release}"
rm -f "${direct_stage}" "${zip_stage}"

cat <<EOF_REPORT
Linux release artifacts generated:
  project_root: ${repo_root}
  version: ${version}
  preset: ${preset}
  architecture: ${arch}
  artifact_root: ${artifact_root}
  archive: ${archive_path}
  latest_archive: ${dist_root}/latest.tar.gz
  latest_binary: ${dist_root}/latest-binary
  latest_run: ${dist_root}/latest-run.sh
  latest: ${dist_root}/latest
  final_root: ${final_root}
  tagged_release: ${tagged_release}
EOF_REPORT
