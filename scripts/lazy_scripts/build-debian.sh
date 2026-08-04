#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/lazy_scripts/build-debian.sh [--preset <preset>]

Incremental Debian/Ubuntu build. Reuses the selected preset build directory.

Options:
  --preset <preset>    Optional CMake preset used by configure and build. Default: dev.
  --help               Show this help.
EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
preset="dev"

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --preset)
      preset="${2}"
      shift 2
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      show_help >&2
      exit 1
      ;;
  esac
done

if [[ -z "${preset}" ]]; then
  echo "Preset cannot be empty." >&2
  exit 1
fi

# shellcheck disable=SC1091
source "${repo_root}/scripts/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root" || exit 1
ssa_native_guard_tools cmake || exit 1
# shellcheck disable=SC1091
source "${repo_root}/scripts/debian-paths.sh"
build_dir="$(debian_build_dir "${repo_root}" "${preset}")"

SSA_BUILD_DIR="${build_dir}" "${repo_root}/tools/configure-dev.sh" "${preset}"
cmake --build "${build_dir}"
