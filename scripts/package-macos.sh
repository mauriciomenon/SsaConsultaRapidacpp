#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  ./scripts/package-macos.sh [--preset <preset>] [--arch <arch>] [--dist-dir <dir>] [--project-root <dir>] [--version <version>] [--skip-tests]

Build and package macOS release artifacts with no arguments.

Defaults:
  Preset: release
  Architecture: uname -m (arm64 or x86_64)
  Artifact dir: dist/macos/<arch>/

Parameters:
  --preset <preset>
  --arch <arch>
  --dist-dir <dir>
  --project-root <dir>
  --version <version>
  --skip-tests

Generated files:
  - ssa_consulta_rapida-<version>-<arch>-macos.zip
  - ssa_consulta_rapida-<version>-<arch>-macos.dmg
  - dist/macos/<arch>/ssa_consulta_rapida-<version>-<arch>-macos/ (artifact root)
  - dist/macos/<arch>/ssa_consulta_rapida-<version>-<arch>-macos/ssa_consulta_rapida.app
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
# shellcheck source=scripts/lib/package_common.sh
source "${script_dir}/lib/package_common.sh"
repo_root="$(package_repo_root_from_script "${BASH_SOURCE[0]}")"
preset="release"
run_tests="true"
arch="$(uname -m)"
dist_root=""
version=""
dmg_stage_root=""
codesign_identity="${SSA_MACOS_CODESIGN_IDENTITY:--}"

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
  dist_root="${repo_root}/dist/macos/${arch}"
fi

if [[ -z "${version}" ]]; then
  version="$(package_project_version "${repo_root}")"
fi
if [[ -z "${version}" ]]; then
  echo "Could not read project version from ${repo_root}/CMakeLists.txt." >&2
  exit 1
fi
build_dir="${repo_root}/build/${preset}"
artifact_name="ssa_consulta_rapida-${version}-${arch}-macos"
artifact_root="${dist_root}/${artifact_name}"
zip_path="${dist_root}/${artifact_name}.zip"
dmg_path="${dist_root}/${artifact_name}.dmg"
staging_id="${artifact_name}.$$.staging"
staged_artifact_root="${dist_root}/.${staging_id}"
staged_zip_path="${dist_root}/.${staging_id}.zip"
staged_dmg_path="${dist_root}/.${staging_id}.dmg"
dmg_stage_root="${staged_artifact_root}/dmg"

cleanup_package_stage() {
  if [[ -n "${dmg_stage_root}" && "${dmg_stage_root}" == "${staged_artifact_root}/dmg" ]]; then
    rm -rf "${dmg_stage_root}"
  fi
  rm -rf "${staged_artifact_root}"
  rm -f "${staged_zip_path}" "${staged_dmg_path}"
}
trap cleanup_package_stage EXIT

"${repo_root}/tools/configure-dev.sh" "${preset}"
cmake --build --preset "${preset}"
if [[ "${run_tests}" == "true" ]]; then
  ctest --preset "${preset}" --output-on-failure
fi

executable="${build_dir}/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida"
app_bundle="${build_dir}/ssa_consulta_rapida.app"
if [[ ! -x "${executable}" ]]; then
  echo "Binary not found: ${executable}" >&2
  exit 1
fi
if ! command -v lipo >/dev/null 2>&1; then
  echo "lipo not found. Cannot verify packaged binary architecture." >&2
  exit 1
fi
binary_arches="$(lipo -archs "${executable}")"
if [[ " ${binary_arches} " != *" ${arch} "* ]]; then
  echo "Requested architecture ${arch}, but ${executable} contains: ${binary_arches}" >&2
  exit 1
fi

mkdir -p "${dist_root}"
rm -rf "${staged_artifact_root}"
rm -f "${staged_zip_path}" "${staged_dmg_path}"
mkdir -p "${staged_artifact_root}"
cp -R "${app_bundle}" "${staged_artifact_root}/"
bundle_copy="${staged_artifact_root}/ssa_consulta_rapida.app"
notices_dir="${bundle_copy}/Contents/Resources/licenses"
mkdir -p "${notices_dir}"
cp "${repo_root}/THIRD_PARTY_NOTICES.md" "${notices_dir}/"
cp "${repo_root}/third_party/tinted-themes/LICENSE" \
  "${notices_dir}/TINTED_SCHEMES_LICENSE.txt"

qml_module_dir="${build_dir}/SsaConsultaRapida"
if [[ ! -d "${qml_module_dir}" ]]; then
  echo "Generated QML module not found: ${qml_module_dir}" >&2
  exit 1
fi

if macdeploy_tool="$(package_resolve_macdeployqt)"; then
  if "${macdeploy_tool}" "${bundle_copy}" -qmldir="${qml_module_dir}"; then
    :
  else
    echo "macdeployqt returned non-zero. Generated bundle may not be self sufficient." >&2
    exit 1
  fi
else
  echo "macdeployqt not found. Self contained macOS package cannot be generated." >&2
  exit 1
fi

sql_drivers_dir="${bundle_copy}/Contents/PlugIns/sqldrivers"
if [[ -d "${sql_drivers_dir}" ]]; then
  while IFS= read -r -d '' sql_driver; do
    if [[ "$(basename "${sql_driver}")" != "libqsqlite.dylib" ]]; then
      rm -f -- "${sql_driver}"
    fi
  done < <(find "${sql_drivers_dir}" -maxdepth 1 -type f -name 'libqsql*.dylib' -print0)
fi

frameworks_dir="${bundle_copy}/Contents/Frameworks"
if [[ -d "${frameworks_dir}" ]]; then
  (
    shopt -s nullglob
    rm -f -- "${frameworks_dir}"/libiodbc*.dylib
    rm -f -- "${frameworks_dir}"/libpq*.dylib
    rm -f -- "${frameworks_dir}"/libmimer*.dylib
  )
fi

codesign --force --deep --sign "${codesign_identity}" "${bundle_copy}"

ditto -c -k --sequesterRsrc --keepParent "${bundle_copy}" "${staged_zip_path}"

if ! command -v hdiutil >/dev/null 2>&1; then
  echo "hdiutil not found. Cannot generate required macOS DMG artifact." >&2
  exit 1
fi
rm -rf "${dmg_stage_root}"
mkdir -p "${dmg_stage_root}"
cp -R "${bundle_copy}" "${dmg_stage_root}/"
ln -s /Applications "${dmg_stage_root}/Applications"
hdiutil create -volname "SSA Consulta Rapida Cpp" -srcfolder "${dmg_stage_root}" -ov -format UDZO "${staged_dmg_path}" >/dev/null
rm -rf "${dmg_stage_root}"
dmg_stage_root=""

cat > "${staged_artifact_root}/run-ssa_consulta_rapida.sh" <<'EOF_RUN'
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${SCRIPT_DIR}/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida" "$@"
EOF_RUN
chmod +x "${staged_artifact_root}/run-ssa_consulta_rapida.sh"

rm -rf "${artifact_root}"
mv "${staged_artifact_root}" "${artifact_root}"
mv -f "${staged_zip_path}" "${zip_path}"
mv -f "${staged_dmg_path}" "${dmg_path}"
final_bundle="${artifact_root}/ssa_consulta_rapida.app"

package_set_latest_link "${dist_root}" "${artifact_name}"
package_set_latest_alias "${dist_root}" "latest.zip" "${artifact_name}.zip"
package_set_latest_alias "${dist_root}" "latest.dmg" "${artifact_name}.dmg"
package_set_latest_alias "${dist_root}" "latest-binary" "${artifact_name}/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida"
package_set_latest_alias "${dist_root}" "latest.app" "${artifact_name}/ssa_consulta_rapida.app"
package_set_latest_alias "${dist_root}" "latest-run.sh" "${artifact_name}/run-ssa_consulta_rapida.sh"

cat <<EOF_REPORT
macOS artifacts generated:
  project_root: ${repo_root}
  version: ${version}
  preset: ${preset}
  architecture: ${arch}
  bundle: ${final_bundle}
  zip: ${zip_path}
  dmg: ${dmg_path}
  latest_zip: ${dist_root}/latest.zip
  latest_dmg: ${dist_root}/latest.dmg
  latest_binary: ${dist_root}/latest-binary
  latest_app: ${dist_root}/latest.app
  latest_run: ${dist_root}/latest-run.sh
  latest: ${dist_root}/latest
EOF_REPORT
