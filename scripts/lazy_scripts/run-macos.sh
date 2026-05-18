#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage:
  scripts/lazy_scripts/run-macos.sh <path-to-ssas.db> [--project-root <dir>] [--preset <preset>] [--config-dir <dir>] [--screenshot <file>]

Run macOS app using explicit params.

Positional:
  <path-to-ssas.db>   Source database path.

Options:
  --project-root <dir>  Override project root.
  --preset <preset>     CMake preset used to locate executable. Default: dev.
  --config-dir <dir>    Optional app --config-dir argument.
  --screenshot <file>   Optional app --screenshot argument.
  --help                Show this help.
EOF
}

if [[ "${1-}" == "--help" || "${1-}" == "-h" ]]; then
  show_help
  exit 0
fi

if [[ $# -lt 1 ]]; then
  show_help >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
db_path="$1"
shift

preset="dev"
project_root="${repo_root}"
config_dir=""
screenshot=""

while [[ $# -gt 0 ]]; do
  case "${1}" in
    --project-root)
      project_root="${2}"
      shift 2
      ;;
    --preset)
      preset="${2}"
      shift 2
      ;;
    --config-dir)
      config_dir="${2}"
      shift 2
      ;;
    --screenshot)
      screenshot="${2}"
      shift 2
      ;;
    *)
      echo "Unknown argument: ${1}" >&2
      show_help >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "${db_path}" ]]; then
  echo "Database file not found: ${db_path}" >&2
  exit 1
fi
if [[ -z "${project_root}" ]]; then
  echo "project_root cannot be empty." >&2
  exit 1
fi

launch_mode="open"
if [[ -n "${screenshot}" ]]; then
  launch_mode="screenshot"
fi

# shellcheck disable=SC1091
# shellcheck source=../smoke-macos-core.sh
source "${script_dir}/../smoke-macos-core.sh"
run_macos_app "${project_root}" "${preset}" "${db_path}" "${config_dir}" "${screenshot}" \
  "${launch_mode}"
