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

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/lib/package_common.sh"
repo_root="$(package_repo_root_from_script "${BASH_SOURCE[0]}")"
preset="release"
run_tests="true"
arch="$(uname -m)"
dist_root=""
version=""
dmg_stage_root=""

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
dmg_stage_root="${artifact_root}/dmg"

cleanup_dmg_stage() {
  if [[ -n "${dmg_stage_root}" && "${dmg_stage_root}" == "${artifact_root}/dmg" ]]; then
    rm -rf "${dmg_stage_root}"
  fi
}
trap cleanup_dmg_stage EXIT

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

mkdir -p "${dist_root}"
rm -rf "${artifact_root}"
mkdir -p "${artifact_root}"
cp -R "${app_bundle}" "${artifact_root}/"
bundle_copy="${artifact_root}/ssa_consulta_rapida.app"

if macdeploy_tool="$(package_resolve_macdeployqt)"; then
  if "${macdeploy_tool}" "${bundle_copy}"; then
    :
  else
    echo "macdeployqt returned non-zero. Generated bundle may not be self sufficient." >&2
    exit 1
  fi
else
  echo "macdeployqt not found. Self contained macOS package cannot be generated." >&2
  exit 1
fi

rm -f "${zip_path}" "${dmg_path}"
ditto -c -k --sequesterRsrc --keepParent "${bundle_copy}" "${zip_path}"

if ! command -v hdiutil >/dev/null 2>&1; then
  echo "hdiutil not found. Cannot generate required macOS DMG artifact." >&2
  exit 1
fi
rm -rf "${dmg_stage_root}"
mkdir -p "${dmg_stage_root}"
cp -R "${bundle_copy}" "${dmg_stage_root}/"
ln -s /Applications "${dmg_stage_root}/Applications"
hdiutil create -volname "SSA Consulta Rapida Cpp" -srcfolder "${dmg_stage_root}" -ov -format UDZO "${dmg_path}" >/dev/null

cat > "${artifact_root}/run-ssa_consulta_rapida.sh" <<'EOF_RUN'
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${SCRIPT_DIR}/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida" "$@"
EOF_RUN
chmod +x "${artifact_root}/run-ssa_consulta_rapida.sh"

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
  bundle: ${bundle_copy}
  zip: ${zip_path}
  dmg: ${dmg_path}
  latest_zip: ${dist_root}/latest.zip
  latest_dmg: ${dist_root}/latest.dmg
  latest_binary: ${dist_root}/latest-binary
  latest_app: ${dist_root}/latest.app
  latest_run: ${dist_root}/latest-run.sh
  latest: ${dist_root}/latest
EOF_REPORT
