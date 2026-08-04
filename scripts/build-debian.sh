#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/build-debian.sh

Build Debian/Ubuntu target using default preset (dev).
Removes build/debian/<arch>/dev before configuring. Use scripts/lazy_scripts/build-debian.sh
for an explicit incremental build.

EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi
if [[ $# -ne 0 ]]; then
  echo "This script is no-argument by default. Use --help for usage." >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
preset="dev"
# shellcheck disable=SC1091
source "${script_dir}/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root" || exit 1
ssa_native_guard_tools rm cmake || exit 1
# shellcheck disable=SC1091
source "${script_dir}/debian-paths.sh"
build_dir="$(debian_build_dir "${repo_root}" "${preset}")"

# Do not inherit Windows temporary directories when invoked from WSL.
export TMPDIR=/tmp
export TMP=/tmp
export TEMP=/tmp

rm -rf "${build_dir}"
SSA_BUILD_DIR="${build_dir}" "${repo_root}/tools/configure-dev.sh" "${preset}"
cmake --build "${build_dir}"
