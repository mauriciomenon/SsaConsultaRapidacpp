#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  ./scripts/package-linux.sh [--preset <preset>] [--arch <arch>] [--dist-dir <dir>] [--project-root <dir>] [--version <version>] [--skip-tests]

Build and package Linux release artifacts as a self contained tarball with no
arguments.

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
  - ssa_consulta_rapida-<version>-<arch>-linux.tar.gz
  - dist/linux/<arch>/ssa_consulta_rapida-<version>-<arch>-linux/ssa_consulta_rapida
  - dist/linux/<arch>/ssa_consulta_rapida-<version>-<arch>-linux/run-ssa_consulta_rapida.sh
  - dist/linux/<arch>/ssa_consulta_rapida-<version>-<arch>-linux/bin/ssa_consulta_rapida (raw binary)
  - dist/linux/<arch>/ssa_consulta_rapida-<version>-<arch>-linux/lib/<runtime libs>
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
archive_path="${dist_root}/${artifact_name}.tar.gz"

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

mkdir -p "${artifact_root}/bin" "${artifact_root}/lib"
rm -f "${archive_path}"
cp "${binary}" "${artifact_root}/bin/"
chmod +x "${artifact_root}/bin/ssa_consulta_rapida"

package_copy_runtime_libraries "${binary}" "${artifact_root}/lib"
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
EOF_REPORT
